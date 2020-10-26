#ifndef RS_CHANNEL_LAYER_H
#define RS_CHANNEL_LAYER_H

#include <assert.h>
#include <stdint.h>
#include <time.h>

#define RS_CHANNEL_LAYER_EOF 1
#define RS_CHANNEL_LAYER_IRR 2
#define RS_CHANNEL_LAYER_BADFCS 3

/*
 * MSB xxxx xxxx xxxx xxxx LSB
 *       |    |   |    |
 *       |    |   +----+-- Different channels (ch_n)
 *       |    +----------- for now fixed at 0x0
 *       +---------------- Different implementations (ch_base - nonzero)
 */
typedef uint16_t rs_channel_t;
typedef uint16_t rs_channel_layer_seq_t;

#include "rs_channel_layer_packet.h"
#include "rs_stat.h"

struct rs_server_state;
struct rs_packet;
struct rs_channel_layer_vtable;
struct rs_channel_layer_packet;
struct rs_channel_info;

struct rs_channel_layer {
    struct rs_server_state *server;
    struct rs_channel_layer_vtable *vtable;

    struct rs_channel_info *channels;

    uint8_t ch_base;
};

void rs_channel_layer_init(struct rs_channel_layer *layer,
                           struct rs_server_state *server,
                           uint8_t ch_base,
                           struct rs_channel_layer_vtable *vtable);
int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel);

/*
 * Positive value indicates success, returns number of bytes
 */
int rs_channel_layer_transmit(struct rs_channel_layer *layer,
                              struct rs_packet *packet, rs_channel_t channel);
/*
 * (*channel) can either be 0 (must be a valid pointer) to receive on last
 * channel, or can be set to define the channel on whicht to listen.
 * Return values:
 *  negative: errors
 *  0: packet / channel placed in args
 *  RS_CHANNEL_LAYER_EOF: No more packets for now
 *
 *  RS_CHANNEL_LAYER_IRR: Received packet we don't care about
 *  RS_CHANNEL_LAYER_BADFCS: Received packet with bad checksum
 */
int rs_channel_layer_receive(struct rs_channel_layer *layer,
                             struct rs_packet **packet, rs_channel_t *channel);

/*
 * Handle channel layer communication (heartbeats)
 */
#define RS_CHANNEL_CMD_DUMMY_SIZE 1
#define RS_CHANNEL_CMD_HEARTBEAT 0xFD
#define RS_CHANNEL_CMD_HEARTBEAT_MSEC 50

void rs_channel_layer_main(struct rs_channel_layer *layer);

struct rs_channel_layer_vtable {
    void (*destroy)(struct rs_channel_layer *layer);

    int (*_transmit)(struct rs_channel_layer *layer, struct rs_packet *packet,
                     rs_channel_t channel);

    int (*_receive)(struct rs_channel_layer *layer,
                    struct rs_channel_layer_packet **packet,
                    rs_channel_t channel);

    int (*ch_n)(struct rs_channel_layer *layer);
};

static inline void rs_channel_layer_destroy(struct rs_channel_layer *layer) {
    (layer->vtable->destroy)(layer);
}

static inline int rs_channel_layer_ch_n(struct rs_channel_layer *layer) {
    return (layer->vtable->ch_n)(layer);
}
rs_channel_t rs_channel_layer_ch(struct rs_channel_layer *layer, int i);
uint16_t rs_channel_layer_extract(struct rs_channel_layer *layer,
                                  rs_channel_t channel);

void rs_channel_layer_close_all_channels(struct rs_channel_layer *layer);
void rs_channel_layer_open_channel(struct rs_channel_layer *layer,
                                   rs_channel_t channel);
void rs_channel_layer_stats_printf(struct rs_channel_layer *layer);

struct rs_channel_info {
    rs_channel_t id;

    int is_in_use;

    struct timespec tx_last_ts;

    rs_channel_layer_seq_t tx_last_seq;
    rs_channel_layer_seq_t rx_last_seq;

    struct rs_stats stats;
};

#endif
