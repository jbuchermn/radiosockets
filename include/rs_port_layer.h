#ifndef RS_PORT_LAYER_H
#define RS_PORT_LAYER_H

#include <stdint.h>
#include <time.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"
#include "rs_stat.h"

#define RS_PORT_LAYER_EOF 1

/*
 *  Port 0 is reserved for port-layer communication
 */
typedef uint16_t rs_port_id_t;

typedef uint16_t rs_port_layer_seq_t;

struct rs_channel_layer;
struct rs_port;
struct rs_port_channel_info;
struct rs_port_layer_packet;

struct rs_port_layer {
    struct rs_server_state *server;

    struct rs_port **ports;
    int n_ports;
};

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_server_state *server);

void rs_port_layer_create_port(struct rs_port_layer *layer, rs_port_id_t port,
                               rs_channel_t bound_to, int owner);

void rs_port_layer_destroy(struct rs_port_layer *layer);

/*
 * Positive value indicates success, returns number of bytes
 */
int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *packet, rs_port_id_t port);

/*
 * (*port) is only set, not read
 * Return values:
 *  negative: errors
 *  0: packet / port placed in args - maybe more packets to go
 *  RS_PORT_LAYER_EOF: done
 */
int rs_port_layer_receive(struct rs_port_layer *layer,
                          struct rs_packet **packet, rs_port_id_t *port);

/*
 * Handle port layer communication (channel-switching / heartbeats / ...)
 */
void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received);

void rs_port_layer_stats_printf(struct rs_port_layer *layer);

int rs_port_layer_switch_channel(struct rs_port_layer *layer, rs_port_id_t port,
                                 rs_channel_t new_channel);

#define RS_PORT_CMD_DUMMY_SIZE 1
#define RS_PORT_CMD_SWITCH_CHANNEL 0xCC
#define RS_PORT_CMD_SWITCH_N_BROADCAST 10
#define RS_PORT_CMD_SWITCH_DT_BROADCAST_MSEC 50

struct rs_port {
    rs_port_id_t id;
    int owner;

    struct timespec tx_last_ts;

    rs_port_layer_seq_t tx_last_seq;
    rs_port_layer_seq_t rx_last_seq;

    rs_channel_t bound_channel;

    struct rs_stats stats;

    struct {
        enum {
            RS_PORT_CMD_SWITCH_NONE,
            RS_PORT_CMD_SWITCH_OWNING,
            RS_PORT_CMD_SWITCH_FOLLOWING,
        } state;
        struct timespec begin;
        struct timespec at;
        int n_broadcasts;
        rs_channel_t new_channel;
    } cmd_switch_state;
};

#endif
