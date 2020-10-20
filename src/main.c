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
#include "rs_server_state.h"

static struct rs_server_state state;

void signal_handler(int sig_num) {
    state.running = 0;
    signal(SIGINT, signal_handler);
}

int main() {
    signal(SIGINT, signal_handler);

    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("radiosocketd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "Starting radiosocketd...");

    state.running = 1;

    struct rs_command_loop command_loop;
    state.command_loop = &command_loop;

    rs_command_loop_init(&command_loop, 512);

    struct rs_channel_layer_pcap layer1_pcap;
    rs_channel_layer_pcap_init(&layer1_pcap, "wlp3s0");

    while (state.running) {
        rs_command_loop_run(&command_loop, &state);

        rs_channel_t channel;
        struct rs_packet *packet;
        if (rs_channel_layer_receive(&layer1_pcap.super, &packet, &channel)) {
            rs_channel_layer_transmit(&layer1_pcap.super, packet, channel);
            rs_packet_destroy(packet);
            free(packet);
        }
    }

    syslog(LOG_NOTICE, "Shutting down radiosocketd...");

    rs_command_loop_destroy(&command_loop);
    rs_channel_layer_destroy(&layer1_pcap.super);

    syslog(LOG_NOTICE, "...done");
    closelog();
    return 0;
}
