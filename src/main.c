#include <getopt.h>
#include <libconfig.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_app_layer.h"
#include "rs_channel_layer_pcap.h"
#include "rs_command_loop.h"
#include "rs_packet.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"

#define MAIN_LOOP_NS 10 /*us*/ * 1000L
/* #define MAIN_PRINT_STATS */
/* #define MAIN_PRINT_STATS_N 1000 */

static struct rs_server_state state;

void signal_handler(int sig_num) {
    state.running = 0;
    signal(SIGINT, signal_handler);
}

int main(int argc, char **argv) {

    struct rs_channel_layer **layers1 = NULL;
    void **layers1_alloc = NULL;
    int n_layers1 = 0;
    struct rs_port_layer layer2;
    struct rs_app_layer layer3;
    struct rs_command_loop command_loop;

    char sock_file[1024] = "/tmp/radiosocketd.sock";
    char conf_file[1024] = "/etc/radiosocketd.conf";
    int verbose = 0;

    static struct option opts[] = {{"config", required_argument, NULL, 'c'},
                                   {"socket", required_argument, NULL, 's'},
                                   {"verbose", no_argument, NULL, 'v'},
                                   {NULL, 0, NULL, 0}};

    int idx;
    int c;
    while ((c = getopt_long(argc, argv, "c:s:v", opts, &idx)) != -1) {
        switch (c) {
        case 's':
            strncpy(sock_file, optarg, sizeof(sock_file) - 1);
            break;
        case 'c':
            strncpy(conf_file, optarg, sizeof(conf_file) - 1);
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            exit(1);
            break;
        }
    }

    /* set up log */
    if (verbose)
        setlogmask(LOG_UPTO(LOG_DEBUG));
    else
        setlogmask(LOG_UPTO(LOG_NOTICE));

    openlog("radiosocketd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "Starting radiosocketd...");

    /* set up state */
    state.running = 1;
    config_init(&state.config);
    if (config_read(&state.config, fopen(conf_file, "r")) != CONFIG_TRUE) {
        printf("%s: %d\n", config_error_text(&state.config),
               config_error_line(&state.config));
        syslog(LOG_ERR, "Could not open config");

        goto error;
    }

    int own, other;
    if (config_lookup_int(&state.config, "own_id", &own) != CONFIG_TRUE) {
        syslog(LOG_ERR, "config: missing own_id");
        config_destroy(&state.config);

        goto error;
    }
    if (config_lookup_int(&state.config, "other_id", &other) != CONFIG_TRUE) {
        syslog(LOG_ERR, "config: missing other_id");
        config_destroy(&state.config);

        goto error;
    }
    state.own_id = own;
    state.other_id = other;

    /* set up channel layers */
    config_setting_t* cc = config_lookup(&state.config, "channels");
    int n_channel_layers =
        cc ? config_setting_length(cc) : 0;
    layers1 = calloc(n_channel_layers, sizeof(void *));
    layers1_alloc = calloc(n_channel_layers, sizeof(void *));
    for (int i = 0; i < n_channel_layers; i++) {
        char p[100];

        int base;
        sprintf(p, "channels.[%d].base", i);
        config_lookup_int(&state.config, p, &base);

        const char *kind = "";
        sprintf(p, "channels.[%d].kind", i);
        config_lookup_string(&state.config, p, &kind);

        if (!strcmp(kind, "pcap")) {
            const char *ifname;
            sprintf(p, "channels.[%d].pcap.ifname", i);
            if (config_lookup_string(&state.config, p, &ifname) !=
                CONFIG_TRUE) {
                syslog(LOG_ERR, "Need to provide ifname for pcap layer");
                goto error;
            }

            int phys;
            sprintf(p, "channels.[%d].pcap.phys", i);
            if (config_lookup_int(&state.config, p, &phys) != CONFIG_TRUE) {
                syslog(LOG_ERR, "Need to provide ifname for pcap layer");
                goto error;
            }

            struct rs_channel_layer_pcap *layer1 =
                calloc(1, sizeof(struct rs_channel_layer_pcap));

            n_layers1++;
            layers1[n_layers1 - 1] = &layer1->super;
            layers1_alloc[n_layers1 - 1] = layer1;

            if (rs_channel_layer_pcap_init(layer1, &state, base, phys,
                                           (char *)ifname)) {
                syslog(LOG_ERR, "Unable to initalize PCAP layer");
                goto error;
            }

        } else {
            syslog(LOG_ERR, "Unknown channel layer: %s", kind);
        }
    }
    state.n_channel_layers = n_layers1;
    state.channel_layers = layers1;

    /* set up port layer */
    rs_port_layer_init(&layer2, &state);
    state.port_layer = &layer2;

    /* set up app layer */
    rs_app_layer_init(&layer3, &state);
    state.app_layer = &layer3;

    /* set up command loop */
    rs_command_loop_init(&command_loop, sock_file);

    struct timespec last_loop;
    clock_gettime(CLOCK_REALTIME, &last_loop);

#ifdef MAIN_PRINT_STATS
    int printf_cnt = 0;
#endif

    /* main loop */
    signal(SIGINT, signal_handler);
    while (state.running) {
        /* Do stuff */
        rs_command_loop_run(&command_loop, &state);

        struct rs_packet *packet;
        rs_port_id_t port;
        while (!rs_port_layer_receive(state.port_layer, &packet, &port)) {
            rs_app_layer_main(state.app_layer, packet, port);
        }
        for (int i = 0; i < state.n_channel_layers; i++) {
            rs_channel_layer_main(state.channel_layers[i]);
        }
        rs_port_layer_main(state.port_layer, NULL);
        rs_app_layer_main(state.app_layer, NULL, 0);

#ifdef MAIN_PRINT_STATS
        /* Print */
        if (++printf_cnt % MAIN_PRINT_STATS_N == 0) {
            printf("\e[1;1H\e[2J============= PORT =============\n");
            /* rs_port_layer_stats_printf(state.port_layer); */
            printf("============ CHANNEL ===========\n");
            for (int i = 0; i < state.n_channel_layers; i++) {
                rs_channel_layer_stats_printf(state.channel_layers[i]);
            }
        }
#endif

        /* Loop limit */
        struct timespec loop;
        clock_gettime(CLOCK_REALTIME, &loop);

        struct timespec sleep = {0};
        long int nsec_diff =
            (loop.tv_sec > last_loop.tv_sec ? 1000000000L : 0) + loop.tv_nsec -
            last_loop.tv_nsec;

        sleep.tv_nsec = nsec_diff > MAIN_LOOP_NS ? 0 : MAIN_LOOP_NS - nsec_diff;
        if (sleep.tv_nsec)
            nanosleep(&sleep, &sleep);

        last_loop = loop;
    }

    /* shutdown */
    syslog(LOG_NOTICE, "Shutting down radiosocketd...");

    rs_port_layer_destroy(&layer2);
    rs_app_layer_destroy(&layer3);
    rs_command_loop_destroy(&command_loop);

error:
    config_destroy(&state.config);

    for (int i = 0; i < n_layers1; i++) {
        rs_channel_layer_destroy(layers1[i]);
        free(layers1_alloc[i]);
    }
    free(layers1);
    free(layers1_alloc);

    syslog(LOG_NOTICE, "...done");
    closelog();
    return 0;
}
