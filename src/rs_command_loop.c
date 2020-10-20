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
#include "rs_server_state.h"

static void handle_command(struct rs_command_payload *command,
                           struct rs_command_response_payload *response,
                           struct rs_server_state *state) {
    if (command->command == 13)
        state->running = 0;
}

static void send_msg(int sock, void *msg, uint32_t msgsize) {
    if (write(sock, msg, msgsize) < 0) {
        syslog(LOG_ERR, "send_message: can't send message.");
        return;
    }
    syslog(LOG_NOTICE, "send_message: successful, %d bytes", msgsize);
    return;
}

void rs_command_loop_init(struct rs_command_loop *loop,
                          unsigned int buffer_size) {
    loop->buffer_size = buffer_size;
    loop->buffer = calloc(buffer_size, sizeof(char));

    /* create socket */
    if ((loop->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        syslog(LOG_ERR, "create_socket: Socket creation failed");
        exit(1);
    }
    syslog(LOG_NOTICE, "create_socket: Socket created");

    /* setup socket configuration */
    struct sockaddr_un addr;
    bzero((char *)&addr, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CONFIG_SOCKET_FILE, sizeof(addr.sun_path) - 1);

    /* rm socket-file */
    remove(addr.sun_path);

    /* bind to socket */
    if (bind(loop->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "create_socket: Bind failed");
        exit(1);
    }
    listen(loop->socket_fd, 3);
    syslog(LOG_NOTICE, "create_socket: Bind done");

    /* put socket into nonblocking mode */
    int flags = fcntl(loop->socket_fd, F_GETFL);
    fcntl(loop->socket_fd, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_NOTICE, "create_socket: Server listening on socket");
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

    syslog(LOG_NOTICE, "Received command...");

    bzero(loop->buffer, loop->buffer_size);
    int nread;
    while ((nread = read(client_socket_fd, loop->buffer, loop->buffer_size)) >
           0) {
        struct rs_command_payload *p =
            (struct rs_command_payload *)loop->buffer;

        syslog(LOG_NOTICE, "...id=%d, command=%d", p->id, p->command);

        struct rs_command_response_payload response;
        response.id = p->id;

        handle_command(p, &response, state);

        send_msg(client_socket_fd, &response,
                 sizeof(struct rs_command_response_payload));
    }
    close(client_socket_fd);
}
