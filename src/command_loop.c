#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server_state.h"
#include "command_loop.h"

void handle_command(struct command_payload* command, struct command_response_payload* response, struct server_state* state){
    if(command->command == 13) state->running = 0;
}


void send_msg(int sock, void* msg, uint32_t msgsize){
    if (write(sock, msg, msgsize) < 0){
        printf("send_message: can't send message.\n");
        return;
    }
    printf("send_message: successful, %d bytes\n", msgsize);
    return;
}

void command_loop_init(struct command_loop* loop, unsigned int buffer_size){
    loop->buffer_size = buffer_size;
    loop->buffer = calloc(buffer_size, sizeof(char));

    /* create socket */
    if ((loop->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        printf("ERROR: Socket creation failed\n");
        exit(1);
    }
    printf("create_socket: Socket created\n");

    /* setup socket configuration */
    struct sockaddr_un addr;
    bzero((char *) &addr, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CONFIG_SOCKET_FILE, sizeof(addr.sun_path)-1);

    /* rm socket-file */
    remove(addr.sun_path);

    /* bind to socket */
    if (bind(loop->socket_fd, (struct sockaddr *)&addr , sizeof(addr)) < 0){
        printf("ERROR: Bind failed\n");
        exit(1);
    }
    listen(loop->socket_fd, 3);
    printf("create_socket: Bind done\n");

    /* put socket into nonblocking mode */
    int flags = fcntl(loop->socket_fd, F_GETFL);
    fcntl(loop->socket_fd, F_SETFL, flags | O_NONBLOCK);

    printf("create_socket: Server listening on socket\n");
}

void command_loop_destroy(struct command_loop* loop){
    close(loop->socket_fd);
    free(loop->buffer);

    loop->socket_fd = 0;
    loop->buffer = NULL;
}

void command_loop_run(struct command_loop* loop, struct server_state* state){
    /* Connect to client (non-blocking) - return if no connection */
    struct sockaddr_un client;
    unsigned int clilen = sizeof(client);
    int client_socket_fd = accept(loop->socket_fd, (struct sockaddr *)&client, &clilen);
    if (client_socket_fd < 0){
        return;
    }

    printf("----------------------------\n");
    printf("New command: ");

    bzero(loop->buffer, loop->buffer_size);
    int nread;
    while ((nread=read(client_socket_fd, loop->buffer, loop->buffer_size)) > 0){
        struct command_payload *p = (struct command_payload*) loop->buffer;

        printf("%d bytes, ", nread);
        printf("id=%d, command=%d\n", p->id, p->command);

        struct command_response_payload response;
        response.id = p->id;

        handle_command(p, &response, state);

        send_msg(client_socket_fd, &response, sizeof(struct command_response_payload));
    }
    printf("----------------------------\n");
    close(client_socket_fd);
}
