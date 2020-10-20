#ifndef RS_PORT_LAYER_H
#define RS_PORT_LAYER_H

#include <stdint.h>

typedef uint16_t rs_port_t;

struct rs_packet;
struct rs_channel_layer;

struct rs_port_layer {};

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_channel_layer *channel_layers,
                        int n_channel_layers);
void rs_port_layer_destroy(struct rs_port_layer *layer);

int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *packet, rs_port_t port);
int rs_port_layer_receive(struct rs_port_layer *layer, struct rs_packet *packet,
                          rs_port_t *port);

#endif
