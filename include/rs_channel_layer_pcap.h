#ifndef RS_CHANNEL_LAYER_PCAP_H
#define RS_CHANNEL_LAYER_PCAP_H

#include "radiotap-library/radiotap.h"
#include <pcap/pcap.h>

#include "rs_channel_layer.h"
#include "rs_packet.h"

#define RS_PCAP_TX_BUFSIZE 2048
#define RS_PCAP_HEADER_CODE_1 0xFE
#define RS_PCAP_HEADER_CODE_2 0xDC

struct nl_sock;
struct nl_cb;

struct rs_channel_layer_pcap {
    struct rs_channel_layer super;

    pcap_t *pcap;
    int nl_id;
    struct nl_sock *nl_socket;
    struct nl_cb *nl_cb;

    uint32_t nl_wiphy;
    uint32_t nl_if;
    char nl_ifname[10];

    uint16_t on_channel; // last two bytes of rs_channel_t
};

/*
 * phys == 0 for first monitor-capable physical device
 * ifname must be set. Depending on this name, either an existing interface is
 * turned to monitor (works on raspbery with patched rtl8188eus - deleting other
 * interface causes a crash and adding a secondary monitor interface does not
 * work) or a new interface is created aand other interfaces are deleted
 */
int rs_channel_layer_pcap_init(struct rs_channel_layer_pcap *layer, int phys,
                               char *ifname);


#endif
