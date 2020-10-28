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
    PACK(buffer, buffer_len, rs_channel_t, packet->channel);
    PACK(buffer, buffer_len, rs_channel_layer_seq_t, packet->seq);
    PACK(buffer, buffer_len, uint16_t, packet->ts);

    if (rs_stats_packed_pack(&packet->stats, buffer, buffer_len))
        goto pack_err;

    /* command */
    PACK(buffer, buffer_len, uint8_t, packet->command);

pack_err:;
}

void rs_channel_layer_packet_init(struct rs_channel_layer_packet *packet,
                                  void *payload_ownership,
                                  struct rs_packet *payload_packet,
                                  uint8_t *payload_data, int payload_data_len) {
    rs_packet_init(&packet->super, payload_ownership, payload_packet,
                   payload_data, payload_data_len);
    packet->super.vtable = &vtable;
}

int rs_channel_layer_packet_unpack(struct rs_channel_layer_packet *packet,
                                   void *payload_ownership,
                                   uint8_t *payload_data,
                                   int payload_data_len) {

    rs_channel_layer_packet_init(packet, payload_ownership, NULL, payload_data,
                                 payload_data_len);

    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_channel_t, &packet->channel);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_channel_layer_seq_t, &packet->seq);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           uint16_t, &packet->ts);

    if (rs_stats_packed_unpack(&packet->stats, &packet->super.payload_data,
                               &packet->super.payload_data_len))
        goto unpack_err;

    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           uint8_t, &packet->command);

    return 0;

unpack_err:
    return -1;
}

static struct rs_packet_vtable vtable = {
    .destroy = &rs_packet_base_destroy,
    .pack = &rs_packet_base_pack,
    .pack_header = &rs_channel_layer_packet_pack_header};
