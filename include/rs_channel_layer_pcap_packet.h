#ifndef RS_CHANNEL_LAYER_PCAP_PACKET_H
#define RS_CHANNEL_LAYER_PCAP_PACKET_H

#include "rs_channel_layer_pcap.h"

#define RS_CHANNEL_LAYER_PCAP_HEADER_SIZE (2 + sizeof(rs_channel_t))

struct rs_channel_layer_pcap_packet {
    struct rs_channel_layer_packet super;
    rs_channel_t channel;
};


void rs_channel_layer_pcap_packet_pack_header(struct rs_packet *super,
                                                     uint8_t **buffer,
                                                     int *buffer_len);
void rs_channel_layer_pcap_packet_init(
    struct rs_channel_layer_pcap_packet *packet, void* payload_ownership,
    struct rs_packet *payload_packet, uint8_t *payload_data,
    int payload_data_len, rs_channel_t channel);

int rs_channel_layer_pcap_packet_unpack(
    struct rs_channel_layer_pcap_packet *packet, void* payload_ownership,
    uint8_t *payload_data, int payload_data_len);

#endif
