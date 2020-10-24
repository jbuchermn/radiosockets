#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <stdint.h>

struct rs_command_loop;
struct rs_channel_layer;
struct rs_port_layer;

/* "MAC" adress */
typedef uint16_t rs_server_id_t;

struct rs_server_state {
    int running;

    /* both ids are fixed and nonzero */
    rs_server_id_t own_id;
    rs_server_id_t other_id;

    /* layers */
    struct rs_channel_layer** channel_layers;
    int n_channel_layers;
    struct rs_port_layer* port_layer;
};

#endif
