#ifndef COMMAND_LOOP_H
#define COMMAND_LOOP_H

#include <stdint.h>

#define CONFIG_SOCKET_FILE "/tmp/radiosockets_conf"

struct rs_server_state;

struct rs_command_loop {
    int socket_fd;

    unsigned int buffer_size;
    char *buffer;
};

void rs_command_loop_init(struct rs_command_loop *loop,
                          unsigned int buffer_size);
void rs_command_loop_run(struct rs_command_loop *loop,
                         struct rs_server_state *state);
void rs_command_loop_destroy(struct rs_command_loop *loop);

#pragma pack(1)

struct rs_command_payload {
    uint32_t id;
    uint32_t command;
};

struct rs_command_response_payload {
    uint32_t id;
};

#pragma pack()

#endif
