#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_channel_layer_pcap.h"
#include "rs_command_loop.h"
#include "rs_packet.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"

static struct rs_server_state state;

void signal_handler(int sig_num) {
    state.running = 0;
    signal(SIGINT, signal_handler);
}

int main(int argc, char **argv) {
    /* parameters */
    rs_channel_t default_channel = 0x1006;
    int phys = -2;               /* invalid */
    rs_server_id_t own = 0;      /* invalid */
    rs_server_id_t other = 0;    /* invalid */
    char ifname[IFNAMSIZ] = {0}; /* invalid */

    static struct option opts[] = {{"phys", required_argument, NULL, 'p'},
                                   {"ifname", required_argument, NULL, 'i'},
                                   {"channel", required_argument, NULL, 'c'},
                                   {"own", required_argument, NULL, 'a'},
                                   {"other", required_argument, NULL, 'b'},
                                   {NULL, 0, NULL, 0}};

    int idx;
    int c;
    while ((c = getopt_long(argc, argv, "a:b:c:i:p:", opts, &idx)) != -1) {
        switch (c) {
        case 'p':
            phys = atoi(optarg);
            break;
        case 'i':
            strcpy(ifname, optarg);
            break;
        case 'c':
            default_channel = strtol(optarg, NULL, 16);
            break;
        case 'a':
            own = strtol(optarg, NULL, 16);
            break;
        case 'b':
            other = strtol(optarg, NULL, 16);
            break;
        default:
            exit(1);
            break;
        }
    }

    if (phys == -2) {
        printf("Invalid phys\n");
        exit(1);
    }
    if (ifname[0] == 0) {
        printf("Invalid ifname\n");
        exit(1);
    }
    if (own == 0) {
        printf("Invalid own id\n");
        exit(1);
    }
    if (other == 0) {
        printf("Invalid other id\n");
        exit(1);
    }

    /* set up state */
    state.running = 1;
    state.own_id = own;
    state.other_id = other;

    /* set up log */
    setlogmask(LOG_UPTO(LOG_DEBUG));
    /* setlogmask(LOG_UPTO(LOG_NOTICE)); */
    openlog("radiosocketd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "Starting radiosocketd...");

    /* set up command loop */
    struct rs_command_loop command_loop;
    rs_command_loop_init(&command_loop, 512);

    /* set up channel layers */
    struct rs_channel_layer_pcap layer1_pcap;
    if (rs_channel_layer_pcap_init(&layer1_pcap, &state, phys, ifname)) {
        return 0;
    }

    struct rs_channel_layer *layer1s[1] = {&layer1_pcap.super};

    /* set up port layer */
    struct rs_port_layer layer2;
    rs_port_layer_init(&layer2, layer1s, 1, default_channel);

    /* main loop */
    signal(SIGINT, signal_handler);
    while (state.running) {
        rs_command_loop_run(&command_loop, &state);

        struct rs_packet *packet;
        rs_port_id_t port;
        while (!rs_port_layer_receive(&layer2, &packet, &port)) {
        }
        rs_port_layer_main(&layer2, NULL);
    }

    /* shutdown */
    syslog(LOG_NOTICE, "Shutting down radiosocketd...");

    rs_command_loop_destroy(&command_loop);
    rs_channel_layer_destroy(&layer1_pcap.super);
    rs_port_layer_destroy(&layer2);

    syslog(LOG_NOTICE, "...done");
    closelog();
    return 0;
}
