#ifndef SERVER_STATE_H
#define SERVER_STATE_H

struct command_loop;

struct server_state {
    int running;
    struct command_loop* command_loop;
};

#endif
