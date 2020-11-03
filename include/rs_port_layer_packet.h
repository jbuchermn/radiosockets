#ifndef RS_PORT_LAYER_PACKET_H
#define RS_PORT_LAYER_PACKET_H

#include "rs_port_layer.h"
#include "rs_stat.h"

#define RS_PORT_LAYER_COMMAND_LENGTH 8

typedef uint16_t rs_port_layer_seq_t;
typedef uint8_t rs_port_layer_frag_t;

struct rs_port_layer_packet {
    struct rs_packet super;

    rs_port_id_t port;
    uint8_t command;
    uint32_t payload_len;

    /* Relevant for fragmented transmit */
    rs_port_layer_seq_t seq;
    rs_port_layer_frag_t frag; 
    rs_port_layer_frag_t n_frag_decoded; /* equals FEC k */
    rs_port_layer_frag_t n_frag_encoded; /* equals FEC m */

    struct rs_stats_packed stats;

    /* Only if command != 0 */
    uint8_t command_payload[RS_PORT_LAYER_COMMAND_LENGTH];
};

void rs_port_layer_packet_init(struct rs_port_layer_packet *packet,
                               void *payload_ownership,
                               struct rs_packet *payload_packet,
                               uint8_t *payload_data, int payload_data_len);
int rs_port_layer_packet_unpack(struct rs_port_layer_packet *packet,
                                struct rs_packet *from_packet);

int rs_port_layer_packet_split(struct rs_port_layer_packet *packet,
                               struct rs_port *port,
                               struct rs_port_layer_packet ***split);

int rs_port_layer_packet_join(struct rs_port_layer_packet *joined,
                              struct rs_port *port,
                              struct rs_port_layer_packet **split, int n_split);

#endif
