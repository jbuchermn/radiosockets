#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_app_layer.h"
#include "rs_command_loop.h"
#include "rs_message.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"

static void handle_command(struct rs_message *command,
                           struct rs_message *answer,
                           struct rs_server_state *state) {
    if (command->header.cmd == RS_MESSAGE_CMD_EXIT) {
        state->running = 0;
        answer->header.cmd = 0;

    } else if (command->header.cmd == RS_MESSAGE_CMD_REPORT) {
        int n_reports =
            1 + state->app_layer->n_connections + state->port_layer->n_ports;
        for (int c = 0; c < state->n_channel_layers; c++) {
            for (int ch = 0;
                 ch < rs_channel_layer_ch_n(state->channel_layers[c]); ch++) {
                if (state->channel_layers[c]->channels[ch].is_in_use) {
                    n_reports++;
                }
            }
        }

        answer->header.len_payload_char = n_reports;
        answer->header.len_payload_int = RS_MESSAGE_CMD_REPORT_N * n_reports;
        answer->header.len_payload_double = RS_MESSAGE_CMD_REPORT_N * n_reports;

        answer->payload_char =
            calloc(answer->header.len_payload_char, sizeof(char));
        answer->payload_int =
            calloc(answer->header.len_payload_int, sizeof(int));
        answer->payload_double =
            calloc(answer->header.len_payload_double, sizeof(double));

        int idx = 0;
        answer->payload_char[idx] = 'U';
        answer->payload_int[idx * RS_MESSAGE_CMD_REPORT_N] = 0;
        answer->payload_double[idx] = state->usage;
        idx++;

        for (int i = 0; i < state->app_layer->n_connections; i++) {
            answer->payload_char[idx] = 'A';
            answer->payload_int[idx * RS_MESSAGE_CMD_REPORT_N] =
                state->app_layer->connections[i]->port;
            answer->payload_double[idx * RS_MESSAGE_CMD_REPORT_N] =
                rs_stat_current(&state->app_layer->connections[i]->stat_in);
            answer->payload_double[idx * RS_MESSAGE_CMD_REPORT_N + 1] =
                rs_stat_current(
                    &state->app_layer->connections[i]->stat_skipped);
            idx++;
        }

        for (int i = 0; i < state->port_layer->n_ports; i++) {
            answer->payload_char[idx] = 'P';
            answer->payload_int[idx * RS_MESSAGE_CMD_REPORT_N] =
                state->port_layer->ports[i]->id;
            answer->payload_int[idx * RS_MESSAGE_CMD_REPORT_N + 1] =
                state->port_layer->ports[i]->bound_channel;

            rs_stats_place(&state->port_layer->ports[i]->stats,
                           answer->payload_double +
                               (idx * RS_MESSAGE_CMD_REPORT_N));

            answer->payload_double[idx * RS_MESSAGE_CMD_REPORT_N +
                                   RS_STATS_PLACE_N] =
                rs_stat_current(
                    &state->port_layer->ports[i]->tx_stats_fec_factor);
            answer->payload_double[idx * RS_MESSAGE_CMD_REPORT_N +
                                   RS_STATS_PLACE_N + 1] =
                rs_stat_current(
                    &state->port_layer->ports[i]->rx_stats_fec_factor);
            idx++;
        }

        for (int c = 0; c < state->n_channel_layers; c++) {
            for (int ch = 0;
                 ch < rs_channel_layer_ch_n(state->channel_layers[c]); ch++) {
                if (state->channel_layers[c]->channels[ch].is_in_use) {
                    answer->payload_char[idx] = 'C';
                    answer->payload_int[idx * RS_MESSAGE_CMD_REPORT_N] =
                        state->channel_layers[c]->channels[ch].id;

                    rs_stats_place(
                        &state->channel_layers[c]->channels[ch].stats,
                        answer->payload_double +
                            (idx * RS_MESSAGE_CMD_REPORT_N));

                    answer->payload_double[idx * RS_MESSAGE_CMD_REPORT_N +
                                           RS_STATS_PLACE_N] =
                        rs_stat_current(
                            &state->channel_layers[c]->channels[ch].tx_stat_dt);

                    idx++;
                }
            }
        }

        answer->header.cmd = 0;

    } else if (command->header.cmd == RS_MESSAGE_CMD_SWITCH_CHANNEL) {
        rs_port_id_t port = (rs_port_id_t)command->payload_int[0];
        rs_channel_t new_channel = (rs_channel_t)command->payload_int[1];

        answer->header.cmd =
            rs_port_layer_switch_channel(state->port_layer, port, new_channel);

    } else if (command->header.cmd == RS_MESSAGE_CMD_UPDATE_PORT) {
        rs_port_id_t port = (rs_port_id_t)command->payload_int[0];
        int max_packet_size = command->payload_int[1];
        int fec_k = command->payload_int[2];
        int fec_m = command->payload_int[3];

        answer->header.cmd = rs_port_layer_update_port(
            state->port_layer, port, max_packet_size, fec_k, fec_m);
    }
}

void rs_command_loop_init(struct rs_command_loop *loop, const char *sock_file) {

    /* create socket */
    if ((loop->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        syslog(LOG_ERR, "command loop: Could not create socket");
        return;
    }

    /* setup socket configuration */
    struct sockaddr_un addr;
    bzero((char *)&addr, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_file, sizeof(addr.sun_path) - 1);

    /* bind to socket */
    if (bind(loop->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "command loop: Could not bind socket");
        return;
    }
    listen(loop->socket_fd, 0);

    /* put socket into nonblocking mode */
    int flags = fcntl(loop->socket_fd, F_GETFL);
    fcntl(loop->socket_fd, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG, "command loop: Server listening on socket %s", sock_file);
}

void rs_command_loop_destroy(struct rs_command_loop *loop) {
    close(loop->socket_fd);
    loop->socket_fd = 0;
}

void rs_command_loop_run(struct rs_command_loop *loop,
                         struct rs_server_state *state) {

    /* Connect to client (non-blocking) - return if no connection */
    struct sockaddr_un client;
    unsigned int clilen = sizeof(client);
    int client_socket_fd =
        accept(loop->socket_fd, (struct sockaddr *)&client, &clilen);

    if (client_socket_fd < 0) {
        return;
    }

    int nread;
    struct rs_message received = {0};
    struct rs_message answer = {0};
    while ((nread = rs_message_recv(&received, client_socket_fd)) > 0) {
        answer.header.id = received.header.id;
        handle_command(&received, &answer, state);

        rs_message_send(&answer, client_socket_fd);

        rs_message_destroy(&answer);
        rs_message_destroy(&received);
        memset(&received, 0, sizeof(struct rs_message));
        memset(&answer, 0, sizeof(struct rs_message));
    }

    close(client_socket_fd);
}
