#ifndef RS_CHANNEL_LAYER_H
#define RS_CHANNEL_LAYER_H

#include <stdint.h>

/*
 * MSB xxxx xxxx xxxx xxxx LSB
 *       |    |   |    |
 *       |    |   +----+-- Different channels (quality indifferent: 0 ==
 * unknown, 1 ... ch_n2) |    +----------- Different available speeds (0 ==
 * unknown, 1 == fastest ... ch_n1)
 *       +---------------- Different implementations (ch_base)
 */
typedef uint16_t rs_channel_t;

struct rs_packet;
struct rs_channel_layer_vtable;

struct rs_channel_layer {
  struct rs_channel_layer_vtable *vtable;
};

int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel);

struct rs_channel_layer_vtable {
  void (*destroy)(struct rs_channel_layer *layer);
  int (*transmit)(struct rs_channel_layer *layer, struct rs_packet *packet,
                  rs_channel_t channel);
  int (*receive)(struct rs_channel_layer *layer, struct rs_packet **packet,
                 rs_channel_t *channel);

  uint8_t (*ch_base)(struct rs_channel_layer *layer);
  int (*ch_n1)(struct rs_channel_layer *layer);
  int (*ch_n2)(struct rs_channel_layer *layer);
};

static inline void rs_channel_layer_destroy(struct rs_channel_layer *layer) {
  (layer->vtable->destroy)(layer);
}
static inline int rs_channel_layer_transmit(struct rs_channel_layer *layer,
                                            struct rs_packet *packet,
                                            rs_channel_t channel) {
  return (layer->vtable->transmit)(layer, packet, channel);
}
static inline int rs_channel_layer_receive(struct rs_channel_layer *layer,
                                           struct rs_packet **packet,
                                           rs_channel_t *channel) {
  return (layer->vtable->receive)(layer, packet, channel);
}
static inline uint8_t rs_channel_ch_base(struct rs_channel_layer *layer) {
  return (layer->vtable->ch_base)(layer);
}
static inline int rs_channel_ch_n1(struct rs_channel_layer *layer) {
  return (layer->vtable->ch_n1)(layer);
}
static inline int rs_channel_ch_n2(struct rs_channel_layer *layer) {
  return (layer->vtable->ch_n2)(layer);
}

#endif
