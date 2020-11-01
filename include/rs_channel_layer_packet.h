#ifndef RS_CHANNEL_LAYER_PACKET_H
#define RS_CHANNEL_LAYER_PACKET_H

#include "rs_channel_layer.h"
#include "rs_packet.h"
#include "rs_stat.h"

struct rs_channel_layer_packet {
    struct rs_packet super;

    rs_channel_t channel;
    rs_channel_layer_seq_t seq;

    struct rs_stats_packed stats;

    /*
     * RS_CHANNEL_LAYER_HEARTBEAT: heartbeat
     * 0: regular packet
     */
    uint8_t command;
};

void rs_channel_layer_packet_pack_header(struct rs_packet *super,
                                         uint8_t **buffer, int *buffer_len);
void rs_channel_layer_packet_init(struct rs_channel_layer_packet *packet,
                                  void *payload_ownership,
                                  struct rs_packet *payload_packet,
                                  uint8_t *payload_data, int payload_data_len);

int rs_channel_layer_packet_unpack(struct rs_channel_layer_packet *packet,
                                   void *payload_ownership,
                                   uint8_t *payload_data, int payload_data_len);

#endif
