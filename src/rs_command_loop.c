#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_command_loop.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"

static void handle_command(struct rs_command_payload *command,
                           struct rs_command_response_payload *response,
                           struct rs_server_state *state) {
    if (command->command == RS_COMMAND_LOOP_CMD_EXIT) {
        state->running = 0;
    } else if (command->command == RS_COMMAND_LOOP_CMD_PORT_STAT) {
        rs_port_id_t port = command->payload_int[0];

        struct rs_port_channel_info *info;
        if (rs_port_layer_get_channel_info(state->port_layer, port, &info)) {
            syslog(LOG_ERR, "CMD_PORT_STAT: Invalid port");
        } else {
            response->payload_double[0] = rs_stat_current(&info->tx_stat_bits);
            response->payload_double[1] =
                rs_stat_current(&info->other_rx_stat_bits);
            response->payload_double[2] =
                rs_stat_current(&info->other_rx_stat_missed);

            response->payload_double[3] = rs_stat_current(&info->rx_stat_bits);
            response->payload_double[4] =
                rs_stat_current(&info->rx_stat_missed);
        }
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
        syslog(LOG_ERR, "create_socket: Socket creation failed");
        exit(1);
    }
    syslog(LOG_DEBUG, "create_socket: Socket created");

    /* setup socket configuration */
    struct sockaddr_un addr;
    bzero((char *)&addr, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_file, sizeof(addr.sun_path) - 1);

    /* rm socket-file */
    remove(addr.sun_path);

    /* bind to socket */
    if (bind(loop->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "create_socket: Bind failed");
        exit(1);
    }
    listen(loop->socket_fd, 3);
    syslog(LOG_DEBUG, "create_socket: Bind done");

    /* put socket into nonblocking mode */
    int flags = fcntl(loop->socket_fd, F_GETFL);
    fcntl(loop->socket_fd, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG, "create_socket: Server listening on socket");
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

    syslog(LOG_DEBUG, "Received connection...");

    int nread;
    while ((nread = read(client_socket_fd, loop->buffer,
                         sizeof(struct rs_command_payload))) > 0) {

        if (nread != sizeof(struct rs_command_payload)) {
            printf("%d != %ld\n", nread, sizeof(struct rs_command_payload));
            syslog(LOG_ERR, "Skipping: Invalid command size");
            continue;
        }


        struct rs_command_payload *p =
            (struct rs_command_payload *)loop->buffer;

        syslog(LOG_DEBUG, "Received valid command...");
        struct rs_command_response_payload response;
        response.id = p->id;

        handle_command(p, &response, state);

        send_msg(client_socket_fd, &response,
                 sizeof(struct rs_command_response_payload));
    }
    close(client_socket_fd);
    syslog(LOG_DEBUG, "...closed connection");
}
