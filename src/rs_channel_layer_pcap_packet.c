#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>

#include "rs_channel_layer_pcap_packet.h"
#include "rs_packet.h"
#include "rs_util.h"

static struct rs_packet_vtable vtable;

void rs_channel_layer_pcap_packet_pack_header(struct rs_packet *super,
                                                     uint8_t **buffer,
                                                     int *buffer_len) {
    struct rs_channel_layer_pcap_packet *packet =
        rs_cast(rs_channel_layer_pcap_packet, super);
    /*
     * TODO: Assumes rs_channel_t to be 2 bytes
     */
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

void rs_channel_layer_pcap_packet_init(
    struct rs_channel_layer_pcap_packet *packet, void* payload_ownership,
    struct rs_packet *payload_packet, uint8_t *payload_data,
    int payload_data_len, rs_channel_t channel) {
    rs_packet_init(&packet->super, payload_ownership, payload_packet, payload_data,
                   payload_data_len);
    packet->super.vtable = &vtable;
    packet->channel = channel;
}

int rs_channel_layer_pcap_packet_unpack(
    struct rs_channel_layer_pcap_packet *packet, void* payload_ownership,
    uint8_t *payload_data, int payload_data_len) {

    if (payload_data_len < 4) {
        return -1;
    }

    /* Initialize */
    rs_channel_layer_pcap_packet_init(packet, payload_ownership, NULL, payload_data,
                                      payload_data_len, 0);

    /* Check header code */
    if (*packet->super.payload_data != RS_PCAP_HEADER_CODE_1) {
        return -1;
    }
    packet->super.payload_data++;
    packet->super.payload_data_len--;
    if (*packet->super.payload_data != RS_PCAP_HEADER_CODE_2) {
        return -1;
    }
    packet->super.payload_data++;
    packet->super.payload_data_len--;

    /* Set channel */
    uint8_t ch_base = (uint8_t)(*packet->super.payload_data);
    packet->super.payload_data++;
    packet->super.payload_data_len--;
    uint8_t ch = (uint8_t)(*packet->super.payload_data);
    packet->super.payload_data++;
    packet->super.payload_data_len--;

    packet->channel = ((uint16_t)ch_base << 8) + ch;

    return 0;
}

static struct rs_packet_vtable vtable = {
    .destroy = &rs_packet_base_destroy,
    .pack = &rs_packet_base_pack,
    .pack_header = &rs_channel_layer_pcap_packet_pack_header};
