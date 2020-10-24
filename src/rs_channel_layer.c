#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "rs_channel_layer.h"

void rs_channel_layer_init(struct rs_channel_layer *layer,
                           struct rs_server_state *server) {
    layer->server = server;
}

int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    if ((uint8_t)(channel >> 12) != rs_channel_layer_ch_base(layer))
        return 0;
    if (rs_channel_layer_extract(layer, channel) >=
        rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "owns_channel: Encountered invalid channel");
        return 0;
    }

    return 1;
}

rs_channel_t rs_channel_layer_ch(struct rs_channel_layer *layer, int i) {
    if (i >= rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "ch: Constructing invalid channel");
    }
    return ((layer->vtable->ch_base)(layer) << 12) + i;
}

uint16_t rs_channel_layer_extract(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    uint16_t res = (uint16_t)(0x00FF & channel);
    if (res >= rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "extract: Encountered invalid channel: %40x / %d",
               channel, res);
    }
    return res;
}
