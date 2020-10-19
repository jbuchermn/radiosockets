#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <pcap/pcap.h>

#include "radiotap-library/radiotap_iter.h"
#include "radiotap-library/platform.h"
#include "rs_util.h"
#include "rs_channel_layer_pcap.h"
#include "rs_packet.h"

static struct rs_channel_layer_vtable vtable;

void rs_channel_layer_pcap_init(struct rs_channel_layer_pcap* layer, char* device_name){
    layer->super.vtable = &vtable;

    /* initialize pcap */
    int err;
    char errbuf[PCAP_ERRBUF_SIZE];

    layer->pcap = pcap_create(device_name, errbuf);
    if(layer->pcap == NULL){
        syslog(LOG_ERR, "Unable to open PCAP interface %s: %s", device_name, errbuf);
        return;
    }
    syslog(LOG_NOTICE, "Opened PCAP interface %s", device_name);

    /* configure pcap */
    if (pcap_set_snaplen(layer->pcap, 4096) !=0) syslog(LOG_ERR, "snaplen failed");
    if (pcap_set_promisc(layer->pcap, 1) != 0) syslog(LOG_ERR, "promisc failed");
    if (pcap_set_rfmon(layer->pcap, 1) != 0) syslog(LOG_ERR, "rfmon failed");
    if (pcap_set_timeout(layer->pcap, -1) !=0) syslog(LOG_ERR, "timeout failed");

    /* activate pcap */
    if ((err = pcap_activate(layer->pcap)) !=0){
        syslog(LOG_ERR, "activate failed: %d", err);
        layer->pcap = NULL;
    }
    else if (pcap_setnonblock(layer->pcap, 1, errbuf) != 0){
        syslog(LOG_ERR, "setnonblock failed: %s", errbuf);
        pcap_close(layer->pcap);
        layer->pcap = NULL;
    }

    /* initialize header */
    /* TODO */

}

void rs_channel_layer_pcap_destroy(struct rs_channel_layer* super){
    struct rs_channel_layer_pcap* layer = rs_cast(rs_channel_layer_pcap, super);
    if(layer->pcap) pcap_close(layer->pcap);
}

int rs_channel_layer_pcap_transmit(struct rs_channel_layer* super, struct rs_packet* packet, rs_channel_t channel){

    struct rs_channel_layer_pcap* layer = rs_cast(rs_channel_layer_pcap, super);
    uint8_t tx_buf[RS_PCAP_TX_BUFSIZE];
    uint8_t* tx_ptr = tx_buf;

    memcpy(tx_ptr, &layer->tx_header, sizeof(struct ieee80211_radiotap_header));

    return 0;
}

int rs_channel_layer_pcap_receive(struct rs_channel_layer* super, struct rs_packet** packet, rs_channel_t* channel){
    struct rs_channel_layer_pcap* layer = rs_cast(rs_channel_layer_pcap, super);
    if(layer->pcap == NULL){
        return 0;
    }

    struct pcap_pkthdr header;
    const uint8_t* radiotap_header = pcap_next(layer->pcap, &header);

    if(radiotap_header){
        struct ieee80211_radiotap_iterator it;
        int status = ieee80211_radiotap_iterator_init(&it, (struct ieee80211_radiotap_header*)radiotap_header, header.caplen, NULL);

        int flags = -1;
        int mcs_known = -1;
        int mcs_flags = -1;
        int mcs = -1;
        int rate = -1;
        int chan = -1;
        int chan_flags = -1;
        int antenna = -1;

        while(status == 0){
            if((status = ieee80211_radiotap_iterator_next(&it))) continue;

            switch (it.this_arg_index){
            case IEEE80211_RADIOTAP_FLAGS:
                flags = *(uint8_t*)(it.this_arg);
                break;
            case IEEE80211_RADIOTAP_MCS:
                mcs_known = *(uint8_t*)(it.this_arg);
                mcs_flags = *(((uint8_t*)(it.this_arg))+1);
                mcs = *(((uint8_t*)(it.this_arg))+2);
                break;
            case IEEE80211_RADIOTAP_RATE:
                rate = *(uint8_t*)(it.this_arg);
                break;
            case IEEE80211_RADIOTAP_CHANNEL:
                chan = get_unaligned((uint16_t*)(it.this_arg));
                chan_flags = get_unaligned(((uint16_t*)(it.this_arg))+1);
                break;
            case IEEE80211_RADIOTAP_ANTENNA:
                antenna = *(uint8_t*)(it.this_arg);
                break;
            default:
                break;
            }
        }

        /* find some criterion to skip frames with not enough info */
        if(antenna == -1) return 0;

        int fcs_ok = 1;
        if(flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_BADFCS)){
            fcs_ok = 0;
        }

        const uint8_t* payload = radiotap_header + it._max_length;
        int payload_len = header.caplen - it._max_length;
        if(flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_FCS)){
            payload_len -= 4;
        }

        syslog(LOG_NOTICE, "Flags: %d, MCS: %02x / %02x / %d, Rate: %d, Channel: %d / %02x, Antenna: %d, FCS: %d, Payload: %db", flags, mcs_known, mcs_flags, mcs, rate, chan, chan_flags, antenna, fcs_ok, payload_len);

        /* TODO */
        *channel = 0;

        uint8_t* payload_copy = calloc(payload_len, sizeof(uint8_t));
        memcpy(payload_copy, payload, payload_len);

        *packet = calloc(1, sizeof(struct rs_packet));
        rs_packet_init(*packet, NULL, payload_copy, payload_len);

        return 1;
    }

    return 0;
}

uint8_t rs_channel_layer_pcap_ch_base(struct rs_channel_layer* super){
    return 0x01;
}

int rs_channel_layer_pcap_ch_n1(struct rs_channel_layer* super){
    return 2;
}

int rs_channel_layer_pcap_ch_n2(struct rs_channel_layer* super){
    return 10;
}

static struct rs_channel_layer_vtable vtable = {
    .destroy = rs_channel_layer_pcap_destroy,
    .transmit = rs_channel_layer_pcap_transmit,
    .receive = rs_channel_layer_pcap_receive,
    .ch_base = rs_channel_layer_pcap_ch_base,
    .ch_n1 = rs_channel_layer_pcap_ch_n1,
    .ch_n2 = rs_channel_layer_pcap_ch_n2,
};
