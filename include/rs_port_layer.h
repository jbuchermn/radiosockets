#ifndef RS_PORT_LAYER_H
#define RS_PORT_LAYER_H

#include <stdint.h>
#include <time.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"
#include "rs_stat.h"

#define RS_PORT_LAYER_EOF 1

/*
 * MSB xxxxxxxx xxxxxxxx LSB
 *       |        |
 *       |        +------- id
 *       +---------------- owner's server_id (the one who opened the port)
 *
 *  Port 0 is reserved for port-layer communication
 */
typedef uint16_t rs_port_id_t;

typedef uint16_t rs_port_seq_t;

struct rs_channel_layer;
struct rs_port;
struct rs_port_channel_info;
struct rs_port_layer_packet;

struct rs_port_layer {
    struct rs_server_state *server;

    /* Does not take ownership */
    struct rs_channel_layer **channel_layers;
    int n_channel_layers;

    /* In general commands can be recevied through all channels - this attribute
     * describes through which we are sending them at the moment */
    rs_channel_t command_channel;

    struct rs_port **ports;
    int n_ports;

    /* TODO: In general bit inefficient... it'd be better to store a linked list
     * of channels in use (but at the moment there are 12 channels in total, so
     * it's okay) */
    struct rs_port_channel_info **infos;
};

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_server_state *server,
                        struct rs_channel_layer **channel_layers,
                        int n_channel_layers, rs_channel_t default_channel);
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

int rs_port_layer_open_port(struct rs_port_layer *layer, uint8_t id,
                            rs_port_id_t *opened_id, rs_channel_t channel);

struct rs_port {
    rs_port_id_t id;
    rs_channel_t bound_channel;

    enum {
        RS_PORT_INITIAL,
        RS_PORT_WAITING_ACK, /* owner is waiting for ack */
        RS_PORT_OPENED,
        RS_PORT_OPEN_FAILED
    } status;
    struct timespec last_try;
    int retry_cnt;
};

#define RS_PORT_CMD_RETRY_CNT 10
#define RS_PORT_CMD_RETRY_MSEC 50

#define RS_PORT_CMD_DUMMY_SIZE 1
#define RS_PORT_CMD_HEARTBEAT_MSEC 20

#define RS_PORT_CMD_HEARTBEAT 0xFD
#define RS_PORT_CMD_OPEN 0xFE
#define RS_PORT_CMD_ACK_OPEN 0x0E

struct rs_port_channel_info {
    rs_channel_t id;

    int is_in_use;

    struct timespec tx_last_ts;
    struct timespec rx_last_ts;

    rs_port_seq_t tx_last_seq;
    rs_port_seq_t rx_last_seq;

    struct rs_stat tx_stat_bits;
    struct rs_stat tx_stat_packets;

    struct rs_stat rx_stat_bits;
    struct rs_stat rx_stat_packets;
    struct rs_stat rx_stat_missed;
    struct rs_stat rx_stat_dt;

    struct rs_stat other_rx_stat_bits;
    struct rs_stat other_rx_stat_packets;
    struct rs_stat other_rx_stat_missed;
    struct rs_stat other_rx_stat_dt;
};

/* TODO: Replace by separate statistics for channels and ports -
 * rs_port_channel_info should be rather private*/
int rs_port_layer_get_channel_info(struct rs_port_layer *layer,
                                   rs_port_id_t port,
                                   struct rs_port_channel_info **info);

#endif
