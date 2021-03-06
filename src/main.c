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
#include "rs_channel_layer_nrf24l01_usb.h"
#include "rs_command_loop.h"
#include "rs_packet.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"
#include "rs_util.h"

/* #define MAIN_PRINT_STATS */

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
    state.usage = 1.;
    state.main_loop_us = 50000L;

    config_init(&state.config);
    if (config_read(&state.config, fopen(conf_file, "r")) != CONFIG_TRUE) {
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
    config_setting_t *cc = config_lookup(&state.config, "channels");
    int n_channel_layers = cc ? config_setting_length(cc) : 0;
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
            sprintf(p, "channels.[%d].pcap", i);
            config_setting_t *conf = config_lookup(&state.config, p);

            struct rs_channel_layer_pcap *layer1 =
                calloc(1, sizeof(struct rs_channel_layer_pcap));

            n_layers1++;
            layers1[n_layers1 - 1] = &layer1->super;
            layers1_alloc[n_layers1 - 1] = layer1;

            if (rs_channel_layer_pcap_init(layer1, &state, base, conf)) {
                syslog(LOG_ERR, "Unable to initalize PCAP layer");
                goto error;
            }

        } else if (!strcmp(kind, "nrf24l01_usb")) {
            sprintf(p, "channels.[%d].nrf24l01_usb", i);
            config_setting_t *conf = config_lookup(&state.config, p);

            struct rs_channel_layer_nrf24l01_usb *layer1 =
                calloc(1, sizeof(struct rs_channel_layer_nrf24l01_usb));

            n_layers1++;
            layers1[n_layers1 - 1] = &layer1->super;
            layers1_alloc[n_layers1 - 1] = layer1;

            if (rs_channel_layer_nrf24l01_usb_init(layer1, &state, base, conf)) {
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

    /* main loop */
    signal(SIGINT, signal_handler);
    while (state.running) {
        struct timespec loop_begin;
        clock_gettime(CLOCK_REALTIME, &loop_begin);
        TIMER_START(main);

        /* Do stuff */
        rs_command_loop_run(&command_loop, &state);

        struct rs_packet *packet = NULL;
        rs_port_id_t port;
        while (!rs_port_layer_receive(state.port_layer, &packet, &port)) {
            rs_app_layer_main(state.app_layer, packet, port);
            rs_packet_destroy(packet);
            free(packet);
            packet = NULL;
        }
        for (int i = 0; i < state.n_channel_layers; i++) {
            rs_channel_layer_main(state.channel_layers[i]);
        }
        rs_port_layer_main(state.port_layer, NULL);
        rs_app_layer_main(state.app_layer, NULL, 0);

#ifdef MAIN_PRINT_STATS
        /* Print */
        EVERY(main_printf, 1000) {
            printf("\e[1;1H\e[2J============= PORT =============\n");
            rs_port_layer_stats_printf(state.port_layer);
            printf("============ CHANNEL ===========\n");
            for (int i = 0; i < state.n_channel_layers; i++) {
                rs_channel_layer_stats_printf(state.channel_layers[i]);
            }
        }
#endif

        TIMER_STOP(main, 0);
        TIMER_PRINT(main, 2);

        /* Loop limit */
        struct timespec loop;
        clock_gettime(CLOCK_REALTIME, &loop);

        long long int nsec_diff =
            (loop.tv_sec > loop_begin.tv_sec ? 1000000000L : 0) + loop.tv_nsec -
            loop_begin.tv_nsec;

        /* Usage calculation and load controlling */
        state.usage =
            (1. - (double)state.main_loop_us / 1000000.) * state.usage +
            (double)state.main_loop_us / 1000000. *
                ((double)nsec_diff / ((double)state.main_loop_us * 1000.));

        EVERY(adjust_main_loop, 100){
            if(state.usage > 0.9){
                if(state.main_loop_us < MAIN_LOOP_US_MAX){
                    state.main_loop_us *= 1.2;
                    state.usage /= 1.2;
                }
            }else if(state.usage < 0.8){
                if(state.main_loop_us > MAIN_LOOP_US_MIN){
                    state.main_loop_us *= 0.95;
                    state.usage /= 0.95;
                }
            }
        }

        if (nsec_diff < (long long int)state.main_loop_us * 1000) {
            struct timespec sleep = {0};
            sleep.tv_nsec = (long long int)state.main_loop_us * 1000 - nsec_diff;
            nanosleep(&sleep, &sleep);
        }
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
