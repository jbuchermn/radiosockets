#ifndef RS_PORT_LAYER_PACKET_H
#define RS_PORT_LAYER_PACKET_H

#include "rs_port_layer.h"
#include "rs_stat.h"

#define RS_PORT_LAYER_COMMAND_LENGTH 8

typedef uint16_t rs_port_layer_seq_t;

struct rs_port_layer_packet {
    struct rs_packet super;

    rs_port_id_t port;
    rs_port_layer_seq_t seq;
    uint16_t ts; /* LSBs of current unix timestamp in milliseconds */
    struct rs_stats_packed stats;

    uint8_t command[RS_PORT_LAYER_COMMAND_LENGTH];
};

void rs_port_layer_packet_init(struct rs_port_layer_packet *packet,
                               void *payload_ownership,
                               struct rs_packet *payload_packet,
                               uint8_t *payload_data, int payload_data_len);
int rs_port_layer_packet_unpack(struct rs_port_layer_packet *packet,
                                struct rs_packet *from_packet);

#endif
