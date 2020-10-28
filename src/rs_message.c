#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_message.h"
#include "rs_util.h"

static int _size(struct rs_message *message) {
    return sizeof(struct rs_message_header) +
           message->header.len_payload_int * sizeof(int) +
           message->header.len_payload_char * sizeof(char) +
           message->header.len_payload_double * sizeof(double);
}

int rs_message_send(struct rs_message *message, int socket_fd) {
    int buffer_len = _size(message);
    uint8_t *buffer = calloc(buffer_len, 1);

    uint8_t *p = buffer;

    memcpy(p, &message->header, sizeof(struct rs_message_header));
    p += sizeof(struct rs_message_header);

    memcpy(p, message->payload_int,
           message->header.len_payload_int * sizeof(int));
    p += message->header.len_payload_int * sizeof(int);
    memcpy(p, message->payload_char,
           message->header.len_payload_char * sizeof(char));
    p += message->header.len_payload_char * sizeof(char);
    memcpy(p, message->payload_double,
           message->header.len_payload_double * sizeof(double));
    p += message->header.len_payload_double * sizeof(double);

    int res = write(socket_fd, buffer, p - buffer);
    free(buffer);
    return res;
}

int rs_message_recv(struct rs_message *message, int socket_fd) {
    int nread =
        read(socket_fd, &message->header, sizeof(struct rs_message_header));

    if (nread != sizeof(struct rs_message_header)) {
        return -1;
    }

    message->payload_int = calloc(message->header.len_payload_int, sizeof(int));
    message->payload_char =
        calloc(message->header.len_payload_char, sizeof(char));
    message->payload_double =
        calloc(message->header.len_payload_double, sizeof(double));

    int p_len = message->header.len_payload_int * sizeof(int) +
                message->header.len_payload_char * sizeof(char) +
                message->header.len_payload_double * sizeof(double);
    nread = 0;
    if(p_len){
        uint8_t *alloc = calloc(p_len, sizeof(uint8_t));
        uint8_t* p = alloc;

        nread = read(socket_fd, p, p_len);
        if (nread == p_len) {
            memcpy(message->payload_int, p, message->header.len_payload_int * sizeof(int));
            p += message->header.len_payload_int * sizeof(int);
            memcpy(message->payload_char, p,
                   message->header.len_payload_char * sizeof(char));
            p += message->header.len_payload_char * sizeof(char);
            memcpy(message->payload_double, p,
                   message->header.len_payload_double * sizeof(double));
            p += message->header.len_payload_double * sizeof(double);

        }

        free(alloc);
    }

    return nread == p_len ? 1 : -1;
}

void rs_message_destroy(struct rs_message *message) {
    free(message->payload_int);
    free(message->payload_double);
    free(message->payload_char);
    message->payload_int = NULL;
    message->payload_double = NULL;
    message->payload_char = NULL;
}
