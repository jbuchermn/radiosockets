#ifndef RS_PORT_LAYER_H
#define RS_PORT_LAYER_H

#include <stdint.h>
#include <time.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"

#define RS_PORT_LAYER_EOF 1

/* Port 0 is reserved for port-layer communication */
typedef uint16_t rs_port_id_t;
typedef uint16_t rs_port_seq_t;

struct rs_channel_layer;
struct rs_port;
struct rs_port_channel_info;
struct rs_port_layer_packet;

struct rs_port_layer {
    /* Does not take ownership */
    struct rs_channel_layer **channel_layers;
    int n_channel_layers;

    struct rs_port *ports;
    int n_ports;

    struct rs_port_channel_info **infos;
};

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_channel_layer **channel_layers,
                        int n_channel_layers, rs_channel_t default_channel);
void rs_port_layer_destroy(struct rs_port_layer *layer);

/*
 * A value of zero 
 */
int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *packet, rs_port_id_t port);

    /*
     * (*port) is only set, not read
     * Return values:
     *  negative: errors
     *  0: packet / port placed in args - maybe more packets to go
     *  RS_PORT_LAYER_EOF
     */
int rs_port_layer_receive(struct rs_port_layer *layer, struct rs_packet **packet,
                          rs_port_id_t *port);

/*
 * Handle port layer communication (channel-switching / heartbeats / ...)
 */
void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received);

struct rs_port {
    rs_port_id_t id;
    rs_channel_t bound_channel;
};

#define RS_PORT_CMD_HEARTBEAT 0xFD

#define RS_PORT_CHANNEL_INFO_HEARTBEAT_MSEC 100
#define RS_PORT_CHANNEL_INFO_N 10
#define RS_PORT_CHANNEL_INFO_DT_MSEC 500

struct rs_port_channel_info {
    rs_channel_t id;

    struct timespec tx_last_ts;
    struct timespec rx_last_ts;

    rs_port_seq_t tx_last_seq;
    rs_port_seq_t rx_last_seq;

    struct timespec stat_t0;
    int tx_stat_bits[RS_PORT_CHANNEL_INFO_N];
    int rx_stat_bits[RS_PORT_CHANNEL_INFO_N];

    int tx_stat_packets[RS_PORT_CHANNEL_INFO_N];
    int rx_stat_packets[RS_PORT_CHANNEL_INFO_N];
};

#endif
