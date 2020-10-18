#ifndef COMMAND_LOOP_H
#define COMMAND_LOOP_H

struct server_state;

struct command_loop {
    int socket_fd;

    unsigned int buffer_size;
    char* buffer;
};

void command_loop_init(struct command_loop* loop, unsigned int buffer_size);
void command_loop_run(struct command_loop* loop, struct server_state* state);
void command_loop_destroy(struct command_loop* loop);

#pragma pack(1)

struct command_payload {
    uint32_t id;
    uint32_t command;
};

struct command_response_payload{
    uint32_t id;
};

#pragma pack()

#endif
