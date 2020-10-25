#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <stdint.h>

#include "rs_channel_layer.h"

struct rs_command_loop;
struct rs_port_layer;

/* "MAC" adress */
typedef uint8_t rs_server_id_t;

struct rs_server_state {
    int running;

    /* both ids are fixed and nonzero */
    rs_server_id_t own_id;
    rs_server_id_t other_id;

    /* layers */
    struct rs_channel_layer **channel_layers;
    int n_channel_layers;
    struct rs_port_layer *port_layer;
};

inline struct rs_channel_layer *
rs_server_channel_layer_for_channel(struct rs_server_state *server,
                                    rs_channel_t channel) {
    for (int i = 0; i < server->n_channel_layers; i++) {
        if (rs_channel_layer_owns_channel(server->channel_layers[i], channel)) {
            return server->channel_layers[i];
        }
    }

    return NULL;
}

#endif
