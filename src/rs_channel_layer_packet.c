#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_channel_layer_packet.h"
#include "rs_packet.h"
#include "rs_util.h"

static struct rs_packet_vtable vtable;

void rs_channel_layer_packet_pack_header(struct rs_packet *super,
                                         uint8_t **buffer, int *buffer_len) {
    struct rs_channel_layer_packet *packet =
        rs_cast(rs_channel_layer_packet, super);

    /* channel */
    if (*buffer_len < sizeof(rs_channel_t))
        return;
    for (int i = sizeof(rs_channel_t) - 1; i >= 0; i--) {
        (**buffer) = (uint8_t)(packet->channel >> (8 * i));
        (*buffer)++;
        (*buffer_len)--;
    }

    /* seq */
    if (*buffer_len < sizeof(rs_channel_layer_seq_t))
        return;
    for (int i = sizeof(rs_channel_layer_seq_t) - 1; i >= 0; i--) {
        (**buffer) = (uint8_t)(packet->seq >> (8 * i));
        (*buffer)++;
        (*buffer_len)--;
    }

    /* command */
    if (*buffer_len < 1)
        return;
    (**buffer) = packet->command;
    (*buffer)++;
    (*buffer_len)--;
}

void rs_channel_layer_packet_init(struct rs_channel_layer_packet *packet,
                                  void *payload_ownership,
                                  struct rs_packet *payload_packet,
                                  uint8_t *payload_data, int payload_data_len) {
    packet->super.vtable = &vtable;
    rs_packet_init(&packet->super, payload_ownership, payload_packet,
                   payload_data, payload_data_len);
}

int rs_channel_layer_packet_unpack(struct rs_channel_layer_packet *packet,
                                   void *payload_ownership,
                                   uint8_t *payload_data,
                                   int payload_data_len) {

    rs_channel_layer_packet_init(packet, payload_ownership, NULL, payload_data,
                                 payload_data_len);

    /* channel */
    if (payload_data_len < sizeof(rs_channel_t))
        return -1;
    packet->channel = 0;
    for (int i = sizeof(rs_channel_t) - 1; i >= 0; i--) {
        packet->channel += ((rs_channel_t)(*payload_data)) << (8 * i);
        packet->super.payload_data++;
        packet->super.payload_data_len--;
    }

    /* seq */
    if (payload_data_len < sizeof(rs_channel_layer_seq_t))
        return -1;
    packet->seq = 0;
    for (int i = sizeof(rs_channel_layer_seq_t) - 1; i >= 0; i--) {
        packet->seq += ((rs_channel_layer_seq_t)(*payload_data)) << (8 * i);
        packet->super.payload_data++;
        packet->super.payload_data_len--;
    }

    /* command */
    if (payload_data_len < 1)
        return -1;
    packet->seq = (*payload_data);
    packet->super.payload_data++;
    packet->super.payload_data_len--;

    return 0;
}

static struct rs_packet_vtable vtable = {
    .destroy = &rs_packet_base_destroy,
    .pack = &rs_packet_base_pack,
    .pack_header = &rs_channel_layer_packet_pack_header};
