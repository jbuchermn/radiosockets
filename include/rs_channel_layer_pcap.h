#ifndef RS_CHANNEL_LAYER_PCAP_H
#define RS_CHANNEL_LAYER_PCAP_H

#include <pcap/pcap.h>
#include "radiotap-library/radiotap.h"

#include "rs_channel_layer.h"

#define RS_PCAP_TX_BUFSIZE 2048

struct rs_channel_layer_pcap {
    struct rs_channel_layer super;

    pcap_t* pcap;
    struct ieee80211_radiotap_header tx_header;
};

void rs_channel_layer_pcap_init(struct rs_channel_layer_pcap* layer, char* device_name);

#endif
