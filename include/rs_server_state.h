#ifndef SERVER_STATE_H
#define SERVER_STATE_H

struct rs_command_loop;

struct rs_server_state {
    int running;
    struct rs_command_loop *command_loop;
};

#endif
