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

int main() {
    int phys = 5;
    char *ifname = "wlan5mon";

    setlogmask(LOG_UPTO(LOG_DEBUG));
    /* setlogmask(LOG_UPTO(LOG_NOTICE)); */

    openlog("radiosocketd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

    syslog(LOG_NOTICE, "Starting radiosocketd...");

    /* command loop */
    struct rs_command_loop command_loop;
    state.command_loop = &command_loop;

    rs_command_loop_init(&command_loop, 512);

    /* channel layers */
    struct rs_channel_layer_pcap layer1_pcap;
    if (rs_channel_layer_pcap_init(&layer1_pcap, phys, ifname)) {
        return 0;
    }

    struct rs_channel_layer *layer1s[1] = {&layer1_pcap.super};

    /* port layer */
    struct rs_port_layer layer2;
    rs_port_layer_init(&layer2, layer1s, 1, 0x1006);

    /* main loop */
    state.running = 1;
    signal(SIGINT, signal_handler);
    while (state.running) {
        rs_command_loop_run(&command_loop, &state);

        struct rs_packet *packet;
        rs_port_id_t port;
        while(!rs_port_layer_receive(&layer2, &packet, &port)){}
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
