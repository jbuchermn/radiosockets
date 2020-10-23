#include <stdio.h>
#include <stdlib.h>

#include "rs_channel_layer.h"

void rs_channel_layer_init(struct rs_channel_layer *layer,
                           struct rs_server_state *server) {
    layer->server = server;
}

int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    return (uint8_t)(channel >> 12) == rs_channel_layer_ch_base(layer);
}

rs_channel_t rs_channel_layer_ch(struct rs_channel_layer *layer, int i) {
    return ((layer->vtable->ch_base)(layer) << 12) + i;
}
