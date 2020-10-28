#ifndef RS_MESSAGE_H
#define RS_MESSAGE_H

#include <stdint.h>

#define RS_MESSAGE_CMD_REPORT 1
#define RS_MESSAGE_CMD_SWITCH_CHANNEL 2
#define RS_MESSAGE_CMD_EXIT 13

struct rs_message {
    struct rs_message_header {
        int id;
        int cmd;

        uint16_t len_payload_int;
        uint16_t len_payload_char;
        uint16_t len_payload_double;
    }__attribute__((packed)) header;

    int* payload_int;
    char* payload_char;
    double* payload_double;
};

int rs_message_send(struct rs_message* message, int socket_fd);
int rs_message_recv(struct rs_message* message, int socket_fd);
void rs_message_destroy(struct rs_message* message);

#endif
