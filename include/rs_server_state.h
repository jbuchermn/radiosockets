#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <stdint.h>

struct rs_command_loop;

/* "MAC" adress */
typedef uint16_t rs_server_id_t;

struct rs_server_state {
    int running;

    /* both ids are fixed */
    rs_server_id_t own_id;
    rs_server_id_t other_id;
};

#endif
