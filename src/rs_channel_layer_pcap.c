#include <errno.h>
#include <pcap/pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "radiotap-library/platform.h"
#include "radiotap-library/radiotap_iter.h"
#include "rs_channel_layer_pcap.h"
#include "rs_packet.h"
#include "rs_util.h"

static struct rs_channel_layer_vtable vtable;
static struct rs_packet_vtable vtable_packet;

void rs_channel_layer_pcap_init(struct rs_channel_layer_pcap *layer,
                                char *device_name) {
    layer->super.vtable = &vtable;

    /* initialize pcap */
    int err;
    char errbuf[PCAP_ERRBUF_SIZE];

    layer->pcap = pcap_create(device_name, errbuf);
    if (layer->pcap == NULL) {
        syslog(LOG_ERR, "Unable to open PCAP interface %s: %s", device_name,
               errbuf);
        return;
    }
    syslog(LOG_NOTICE, "Opened PCAP interface %s", device_name);

    /* configure pcap */
    if (pcap_set_snaplen(layer->pcap, 4096) != 0)
        syslog(LOG_ERR, "snaplen failed");
    if (pcap_set_promisc(layer->pcap, 1) != 0)
        syslog(LOG_ERR, "promisc failed");
    if (pcap_set_rfmon(layer->pcap, 1) != 0)
        syslog(LOG_ERR, "rfmon failed");
    if (pcap_set_timeout(layer->pcap, -1) != 0)
        syslog(LOG_ERR, "timeout failed");

    /* activate pcap */
    if ((err = pcap_activate(layer->pcap)) != 0) {
        syslog(LOG_ERR, "activate failed: %d", err);
        layer->pcap = NULL;
    } else if (pcap_setnonblock(layer->pcap, 1, errbuf) != 0) {
        syslog(LOG_ERR, "setnonblock failed: %s", errbuf);
        pcap_close(layer->pcap);
        layer->pcap = NULL;
    }
}

void rs_channel_layer_pcap_destroy(struct rs_channel_layer *super) {
    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);
    if (layer->pcap)
        pcap_close(layer->pcap);
}

static uint8_t tx_radiotap_header[] __attribute__((unused)) = {
    0x00, // it_version
    0x00, // it_pad

    0x0d, 0x00, // it_len

    // it_present
    // bits 7-0, 15-8, 23-16, 31-24
    // set CHANNEL: bit 3
    // set TX_FLAGS: bit 15
    // set MCS: bit 19
    0x08, 0x80, 0x08, 0x00,

    // CHANNEL
    // u16 frequency (MHz), u16 flags
    // frequency: to be set
    // flags 7-0, 15-8
    // set Dynamic CCK-OFDM channel: bit 10
    // set 2GHz channel: bit 7
    0x00, 0x00, 0x80, 0x04,

    // TX_FLAGS
    // u16 flags 7-0, 15-8
    // set NO_ACK: bit 3
    0x08, 0x00,

    // MCS
    // u8 known, u8 flags, u8 mcs
    // known: Guard Interval, Bandwidth, MCS, STBC, FEC
    // flags: _xx1_gbb, xx: STBC, g: GI, bb: bandwidth
    (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW |
     IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC |
     IEEE80211_RADIOTAP_MCS_HAVE_FEC),
    0x10, 0x00};

int rs_channel_layer_pcap_transmit(struct rs_channel_layer *super,
                                   struct rs_packet *packet,
                                   rs_channel_t channel) {
    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);

    if (!rs_channel_layer_owns_channel(super, channel)) {
        syslog(LOG_ERR, "Attempting to send packet through wrong channel");
        return 0;
    }

    uint8_t tx_buf[RS_PCAP_TX_BUFSIZE];
    uint8_t *tx_ptr = tx_buf;
    int tx_len = RS_PCAP_TX_BUFSIZE;

    memcpy(tx_ptr, tx_radiotap_header, sizeof(tx_radiotap_header));

    // Set frequency
    uint16_t freq = 2437;
    tx_ptr[8] = (uint8_t)freq;
    tx_ptr[9] = (uint8_t)(freq >> 8);

    // Set MCS
    // https://en.wikipedia.org/wiki/IEEE_802.11n-2009#Data_rates
    tx_ptr[15] |= IEEE80211_RADIOTAP_MCS_BW_40;
    tx_ptr[15] |= IEEE80211_RADIOTAP_MCS_SGI;
    tx_ptr[16] = 10;

    tx_ptr += sizeof(tx_radiotap_header);
    tx_len -= sizeof(tx_radiotap_header);

    struct rs_channel_layer_pcap_packet packed_packet;
    rs_channel_layer_pcap_packet_init(&packed_packet, packet, 0, 0, channel);
    rs_packet_pack(&packed_packet.super, &tx_ptr, &tx_len);

    if (pcap_inject(layer->pcap, tx_buf, tx_ptr - tx_buf) != tx_ptr - tx_buf) {
        syslog(LOG_ERR, "Could not inject packet");
    }

    return 0;
}

int rs_channel_layer_pcap_receive(struct rs_channel_layer *super,
                                  struct rs_packet **packet,
                                  rs_channel_t *channel) {
    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);
    if (layer->pcap == NULL) {
        return 0;
    }

    struct pcap_pkthdr header;
    const uint8_t *radiotap_header = pcap_next(layer->pcap, &header);

    if (radiotap_header) {
        struct ieee80211_radiotap_iterator it;
        int status = ieee80211_radiotap_iterator_init(
            &it, (struct ieee80211_radiotap_header *)radiotap_header,
            header.caplen, NULL);

        int flags = -1;
        int mcs_known = -1;
        int mcs_flags = -1;
        int mcs = -1;
        int rate = -1;
        int chan = -1;
        int chan_flags = -1;
        int antenna = -1;

        while (status == 0) {
            if ((status = ieee80211_radiotap_iterator_next(&it)))
                continue;

            switch (it.this_arg_index) {
            case IEEE80211_RADIOTAP_FLAGS:
                flags = *(uint8_t *)(it.this_arg);
                break;
            case IEEE80211_RADIOTAP_MCS:
                mcs_known = *(uint8_t *)(it.this_arg);
                mcs_flags = *(((uint8_t *)(it.this_arg)) + 1);
                mcs = *(((uint8_t *)(it.this_arg)) + 2);
                break;
            case IEEE80211_RADIOTAP_RATE:
                rate = *(uint8_t *)(it.this_arg);
                break;
            case IEEE80211_RADIOTAP_CHANNEL:
                chan = get_unaligned((uint16_t *)(it.this_arg));
                chan_flags = get_unaligned(((uint16_t *)(it.this_arg)) + 1);
                break;
            case IEEE80211_RADIOTAP_ANTENNA:
                antenna = *(uint8_t *)(it.this_arg);
                break;
            default:
                break;
            }
        }

        /* criteria to skip frames with not enough info */
        if (antenna == -1)
            return 0;

        if (flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_BADFCS)) {
            syslog(LOG_DEBUG, "Received bad FCS packet");
            return 0;
        }

        const uint8_t *payload = radiotap_header + it._max_length;
        int payload_len = header.caplen - it._max_length;
        if (flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_FCS)) {
            payload_len -= 4;
        }

        syslog(LOG_DEBUG,
               "Flags: %d, MCS: %02x / %02x / %d, Rate: %d, Channel: %d / "
               "%04x, Antenna: %d, Payload: %db",
               flags, mcs_known, mcs_flags, mcs, rate, chan, chan_flags,
               antenna, payload_len);

        uint8_t *payload_copy = calloc(payload_len, sizeof(uint8_t));
        memcpy(payload_copy, payload, payload_len);

        struct rs_channel_layer_pcap_packet pcap_packet;
        if (rs_channel_layer_pcap_packet_unpack(&pcap_packet, payload_copy,
                                                payload_len)) {
            syslog(
                LOG_DEBUG,
                "Received packet which could not be unpacked on channel layer");
            free(payload_copy);
            return 0;
        }

        if (!rs_channel_layer_owns_channel(&layer->super,
                                           pcap_packet.channel)) {
            syslog(LOG_DEBUG, "Received packet on channel without ownership");
            free(payload_copy);
            return 0;
        }

        *packet = calloc(1, sizeof(struct rs_packet));
        rs_packet_init(*packet, pcap_packet.super.payload_packet,
                       pcap_packet.super.payload_data,
                       pcap_packet.super.payload_data_len);
        *channel = pcap_packet.channel;

        return 1;
    }

    return 0;
}

uint8_t rs_channel_layer_pcap_ch_base(struct rs_channel_layer *super) {
    return 0x01;
}

int rs_channel_layer_pcap_ch_n1(struct rs_channel_layer *super) { return 2; }

int rs_channel_layer_pcap_ch_n2(struct rs_channel_layer *super) { return 10; }

static struct rs_channel_layer_vtable vtable = {
    .destroy = rs_channel_layer_pcap_destroy,
    .transmit = rs_channel_layer_pcap_transmit,
    .receive = rs_channel_layer_pcap_receive,
    .ch_base = rs_channel_layer_pcap_ch_base,
    .ch_n1 = rs_channel_layer_pcap_ch_n1,
    .ch_n2 = rs_channel_layer_pcap_ch_n2,
};

static void rs_channel_layer_pcap_packet_pack_header(struct rs_packet *super,
                                                     uint8_t **buffer,
                                                     int *buffer_len) {
    struct rs_channel_layer_pcap_packet *packet =
        rs_cast(rs_channel_layer_pcap_packet, super);
    if (*buffer_len < 4) {
        syslog(LOG_ERR, "pack: buffer too short");
        return;
    }

    /* header code */
    (**buffer) = RS_PCAP_HEADER_CODE_1;
    (*buffer)++;
    (*buffer_len)--;
    (**buffer) = RS_PCAP_HEADER_CODE_2;
    (*buffer)++;
    (*buffer_len)--;

    /* channel */
    (**buffer) = (uint8_t)(packet->channel >> 8);
    (*buffer)++;
    (*buffer_len)--;

    (**buffer) = (uint8_t)(packet->channel % 256);
    (*buffer)++;
    (*buffer_len)--;
}

int rs_channel_layer_pcap_packet_unpack(
    struct rs_channel_layer_pcap_packet *packet, uint8_t *payload_data,
    int payload_data_len) {

    if (payload_data_len < 4) {
        return -1;
    }

    /* Initialize */
    rs_channel_layer_pcap_packet_init(packet, NULL, payload_data,
                                      payload_data_len, 0);
    packet->super.payload_owner = 0;

    /* Check header code */
    if (*payload_data != RS_PCAP_HEADER_CODE_1) {
        return -2;
    }
    (*packet->super.payload_data)++;
    packet->super.payload_data_len--;
    if (*payload_data != RS_PCAP_HEADER_CODE_2) {
        return -2;
    }
    (*packet->super.payload_data)++;
    packet->super.payload_data_len--;

    /* Set channel */
    uint8_t ch_base = (uint8_t)(*packet->super.payload_data);
    (*packet->super.payload_data)++;
    packet->super.payload_data_len--;
    uint8_t ch = (uint8_t)(*packet->super.payload_data);
    (*packet->super.payload_data)++;
    packet->super.payload_data_len--;

    packet->channel = ((uint16_t)ch_base << 8) + ch;

    return 0;
}

void rs_channel_layer_pcap_packet_init(
    struct rs_channel_layer_pcap_packet *packet,
    struct rs_packet *payload_packet, uint8_t *payload_data,
    int payload_data_len, rs_channel_t channel) {
    rs_packet_init(&packet->super, payload_packet, payload_data,
                   payload_data_len);
    packet->super.vtable = &vtable_packet;
    packet->channel = channel;
}

static struct rs_packet_vtable vtable_packet = {
    .destroy = &rs_packet_base_destroy,
    .pack = &rs_packet_base_pack,
    .pack_header = &rs_channel_layer_pcap_packet_pack_header};
