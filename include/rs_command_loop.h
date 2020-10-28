#ifndef RS_COMMAND_LOOP_H
#define RS_COMMAND_LOOP_H

#include <stdint.h>

struct rs_server_state;
struct rs_port_layer;

struct rs_command_loop {
    int socket_fd;
};


void rs_command_loop_init(struct rs_command_loop *loop, const char *sock_file);
void rs_command_loop_run(struct rs_command_loop *loop,
                         struct rs_server_state *state);
void rs_command_loop_destroy(struct rs_command_loop *loop);


#endif
