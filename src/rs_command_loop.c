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
#include "rs_port_layer.h"
#include "rs_server_state.h"

static void handle_command(struct rs_command_payload *command,
                           struct rs_command_response_payload *response,
                           struct rs_server_state *state) {
    if (command->command == RS_COMMAND_LOOP_CMD_EXIT) {
        state->running = 0;

    } else if (command->command == RS_COMMAND_LOOP_CMD_REPORT) {
        int idx = 0;
        for (int i = 0; i < state->port_layer->n_ports; i++) {
            if ((idx + 1) * RS_STATS_PLACE_N > RS_COMMAND_LOOP_PAYLOAD_MAX)
                break;

            response->payload_char[idx] = 'P';
            response->payload_int[idx] = state->port_layer->ports[i]->id;

            rs_stats_place(&state->port_layer->ports[i]->stats,
                           response->payload_double + (idx * RS_STATS_PLACE_N));
            idx++;
        }

        for (int c = 0; c < state->n_channel_layers; c++) {
            for (int ch = 0;
                 ch < rs_channel_layer_ch_n(state->channel_layers[c]); ch++) {
                if (state->channel_layers[c]->channels[ch].is_in_use) {
                    response->payload_char[idx] = 'C';
                    response->payload_int[idx] =
                        state->channel_layers[c]->channels[ch].id;

                    rs_stats_place(
                        &state->channel_layers[c]->channels[ch].stats,
                        response->payload_double + (idx * RS_STATS_PLACE_N));

                    idx++;
                }
            }
        }

    } else if (command->command == RS_COMMAND_LOOP_CMD_SWITCH_CHANNEL) {
        rs_port_id_t port = (rs_port_id_t)command->payload_int[1];
        rs_channel_t new_channel = (rs_channel_t)command->payload_int[2];

        response->payload_int[0] =
            rs_port_layer_switch_channel(state->port_layer, port, new_channel);
    }
}

static void send_msg(int sock, void *msg, uint32_t msgsize) {
    if (write(sock, msg, msgsize) < 0) {
        return;
    }
}

void rs_command_loop_init(struct rs_command_loop *loop, const char *sock_file) {
    loop->buffer = calloc(1, sizeof(struct rs_command_payload));

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

    /* rm socket-file */
    remove(addr.sun_path);

    /* bind to socket */
    if (bind(loop->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "command loop: Could not bind socket");
        return;
    }
    listen(loop->socket_fd, 3);

    /* put socket into nonblocking mode */
    int flags = fcntl(loop->socket_fd, F_GETFL);
    fcntl(loop->socket_fd, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG, "command loop: Server listening on socket");
}

void rs_command_loop_destroy(struct rs_command_loop *loop) {
    close(loop->socket_fd);
    free(loop->buffer);

    loop->socket_fd = 0;
    loop->buffer = NULL;
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
    while ((nread = read(client_socket_fd, loop->buffer,
                         sizeof(struct rs_command_payload))) > 0) {

        if (nread != sizeof(struct rs_command_payload)) {
            continue;
        }

        struct rs_command_payload *p =
            (struct rs_command_payload *)loop->buffer;

        struct rs_command_response_payload response;
        memset(&response, 0, sizeof(response));
        response.id = p->id;

        handle_command(p, &response, state);

        send_msg(client_socket_fd, &response,
                 sizeof(struct rs_command_response_payload));
    }
    close(client_socket_fd);
}
