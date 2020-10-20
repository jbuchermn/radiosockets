#include <stdio.h>
#include <stdlib.h>

#include "rs_channel_layer.h"

int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    return (uint8_t)(channel >> 8) == rs_channel_ch_base(layer);
}
