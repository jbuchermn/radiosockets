#ifndef RS_PORT_LAYER_PACKET_H
#define RS_PORT_LAYER_PACKET_H

#include "rs_port_layer.h"

#define RS_PORT_LAYER_COMMAND_LENGTH 4

struct rs_port_layer_packet {
    struct rs_packet super;

    rs_port_id_t port;
    rs_port_seq_t seq;

    /* is set in init() to nanosecond timestamp */
    uint64_t ts_sent;

    uint16_t rx_bitrate; /* in bps */
    uint16_t rx_missed; /* normalized to 10000 */
    uint16_t rx_dt; /* in milliseconds */

    uint8_t command[RS_PORT_LAYER_COMMAND_LENGTH];
};

void rs_port_layer_packet_init(struct rs_port_layer_packet *packet,
                               void *payload_ownership,
                               struct rs_packet *payload_packet,
                               uint8_t *payload_data, int payload_data_len);
int rs_port_layer_packet_unpack(struct rs_port_layer_packet *packet,
                                struct rs_packet *from_packet);

#endif
