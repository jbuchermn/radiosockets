#ifndef RS_PORT_LAYER_H
#define RS_PORT_LAYER_H

#include "zfec/zfec/fec.h"
#include <stdint.h>
#include <time.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"
#include "rs_stat.h"

#define RS_PORT_LAYER_EOF 1

typedef uint8_t rs_port_id_t;

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
                               rs_channel_t bound_to, int owner,
                               int max_packet_size, int fec_k, int fec_m);

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
int rs_port_layer_update_port(struct rs_port_layer *layer, rs_port_id_t port,
                              int max_packet_size, int fec_k, int fec_m);

#define RS_PORT_CMD_DUMMY_SIZE 10

#define RS_PORT_CMD_SWITCH_CHANNEL 0xCC
#define RS_PORT_CMD_REQUEST_SWITCH_CHANNEL 0xC0
#define RS_PORT_CMD_SWITCH_N_BROADCAST 10
#define RS_PORT_CMD_SWITCH_DT_BROADCAST_MSEC 50

#define RS_PORT_CMD_HEARTBEAT 0xDD
#define RS_PORT_CMD_HEARTBEAT_MSEC 100

struct rs_port {
    rs_port_id_t id;
    int owner;

    struct timespec tx_last_ts;

    rs_port_layer_seq_t tx_last_seq;
    rs_port_layer_seq_t rx_last_seq;

    rs_channel_t bound_channel;

    struct rs_stats stats;
    struct rs_stat tx_stats_fec_factor;
    struct rs_stat rx_stats_fec_factor;

    struct {
        rs_port_layer_seq_t seq;
        int n_frag;
        int n_frag_received;
        struct rs_port_layer_packet **fragments;
    } frag_buffer;

    int tx_max_packet_size;
    unsigned short tx_fec_m;
    unsigned short tx_fec_k;
    fec_t *tx_fec;

    unsigned short rx_fec_m;
    unsigned short rx_fec_k;
    fec_t *rx_fec;

    struct {
        enum {
            RS_PORT_CMD_SWITCH_NONE,
            RS_PORT_CMD_SWITCH_OWNING,
            RS_PORT_CMD_SWITCH_FOLLOWING,
            RS_PORT_CMD_SWITCH_REQUESTING
        } state;
        struct timespec begin;
        struct timespec at;
        int n_broadcasts;
        rs_channel_t new_channel;
    } cmd_switch_state;
};

void rs_port_setup_tx_fec(struct rs_port *port, int max_packet_size, int k, int m);
void rs_port_setup_rx_fec(struct rs_port *port, int k, int m);

#endif
