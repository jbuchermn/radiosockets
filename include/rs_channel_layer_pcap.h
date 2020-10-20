#ifndef RS_CHANNEL_LAYER_PCAP_H
#define RS_CHANNEL_LAYER_PCAP_H

#include "radiotap-library/radiotap.h"
#include <pcap/pcap.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"

#define RS_PCAP_TX_BUFSIZE 2048

struct rs_channel_layer_pcap {
    struct rs_channel_layer super;

    pcap_t *pcap;
    struct ieee80211_radiotap_header tx_header;
};

void rs_channel_layer_pcap_init(struct rs_channel_layer_pcap *layer,
                                char *device_name);

struct rs_channel_layer_pcap_packet {
    struct rs_packet super;

    rs_channel_t channel;
};

void rs_channel_layer_pcap_packet_init(
    struct rs_channel_layer_pcap_packet *packet,
    struct rs_packet *payload_packet, uint8_t *payload_data,
    int payload_data_len, rs_channel_t channel);
void rs_channel_layer_pcap_packet_unpack(
    struct rs_channel_layer_pcap_packet *packet, uint8_t *payload_data,
    int payload_data_len);

#endif
