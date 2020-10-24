#ifndef COMMAND_LOOP_H
#define COMMAND_LOOP_H

#include <stdint.h>

struct rs_server_state;
struct rs_port_layer;

struct rs_command_loop {
    int socket_fd;

    unsigned int buffer_size;
    uint8_t *buffer;
};

void rs_command_loop_init(struct rs_command_loop *loop, const char *sock_file,
                          unsigned int buffer_size);
void rs_command_loop_run(struct rs_command_loop *loop,
                         struct rs_server_state *state);
void rs_command_loop_destroy(struct rs_command_loop *loop);

#pragma pack(1)

#define RS_COMMAND_LOOP_PAYLOAD_MAX 100

#define RS_COMMAND_LOOP_CMD_PORT_STAT 1
#define RS_COMMAND_LOOP_CMD_EXIT 13

struct rs_command_payload {
    uint32_t id;
    uint32_t command;

    int payload_int[RS_COMMAND_LOOP_PAYLOAD_MAX];
    char payload_char[RS_COMMAND_LOOP_PAYLOAD_MAX];
    double payload_double[RS_COMMAND_LOOP_PAYLOAD_MAX];
};

struct rs_command_response_payload {
    uint32_t id;

    int payload_int[RS_COMMAND_LOOP_PAYLOAD_MAX];
    char payload_char[RS_COMMAND_LOOP_PAYLOAD_MAX];
    double payload_double[RS_COMMAND_LOOP_PAYLOAD_MAX];
};

#pragma pack()

#endif
