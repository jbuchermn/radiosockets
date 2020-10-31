#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_packet.h"
#include "rs_port_layer_packet.h"
#include "rs_util.h"

static struct rs_packet_vtable vtable;

int rs_port_layer_packet_len_header(struct rs_packet *super) {
    struct rs_port_layer_packet *packet = rs_cast(rs_port_layer_packet, super);
    return sizeof(uint8_t) + sizeof(rs_port_id_t) +
                       sizeof(rs_port_layer_seq_t) +
                       sizeof(rs_port_layer_frag_t) +
                       sizeof(rs_port_layer_frag_t) + sizeof(uint16_t) +
                       rs_stats_packed_len(&packet->stats) + (packet->command !=
                   0
               ? RS_PORT_LAYER_COMMAND_LENGTH * sizeof(uint8_t)
               : 0);
}

void rs_port_layer_packet_pack_header(struct rs_packet *super, uint8_t **buffer,
                                      int *buffer_len) {
    struct rs_port_layer_packet *packet = rs_cast(rs_port_layer_packet, super);

    /* command */
    PACK(buffer, buffer_len, uint8_t, packet->command);
    PACK(buffer, buffer_len, rs_port_id_t, packet->port);
    PACK(buffer, buffer_len, rs_port_layer_seq_t, packet->seq);
    PACK(buffer, buffer_len, rs_port_layer_frag_t, packet->frag);
    PACK(buffer, buffer_len, rs_port_layer_frag_t, packet->n_frag);
    PACK(buffer, buffer_len, uint16_t, packet->ts);

    if (rs_stats_packed_pack(&packet->stats, buffer, buffer_len))
        return;

    if (packet->command != 0) {
        for (int i = 0; i < RS_PORT_LAYER_COMMAND_LENGTH; i++) {
            PACK(buffer, buffer_len, uint8_t, packet->command_payload[i]);
        }
    }

pack_err:;
}

void rs_port_layer_packet_init(struct rs_port_layer_packet *packet,
                               void *payload_ownership,
                               struct rs_packet *payload_packet,
                               uint8_t *payload_data, int payload_data_len) {
    rs_packet_init(&packet->super, payload_ownership, payload_packet,
                   payload_data, payload_data_len);
    packet->super.vtable = &vtable;
    packet->command = 0;
    packet->port = 0;
    packet->seq = 0;
    packet->frag = 0;
    packet->n_frag = 1;
    memset(packet->command_payload, 0, sizeof(packet->command_payload));
}

int rs_port_layer_packet_unpack(struct rs_port_layer_packet *packet,
                                struct rs_packet *from_packet) {

    /* Initialize */
    rs_port_layer_packet_init(packet, from_packet->payload_ownership, NULL,
                              from_packet->payload_data,
                              from_packet->payload_data_len);

    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           uint8_t, &packet->command);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_id_t, &packet->port);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_seq_t, &packet->seq);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_frag_t, &packet->frag);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_frag_t, &packet->n_frag);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           uint16_t, &packet->ts);

    if (rs_stats_packed_unpack(&packet->stats, &packet->super.payload_data,
                               &packet->super.payload_data_len))
        goto unpack_err;

    /* Possibly set command_payload */
    if (packet->command != 0) {
        for (int i = 0; i < RS_PORT_LAYER_COMMAND_LENGTH; i++) {
            UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
                   uint8_t, &packet->command_payload[i]);
        }
    }

    /* Successfully unpacked --> steal data and destroy from_packet */
    from_packet->payload_ownership = NULL;
    rs_packet_destroy(from_packet);

    return 0;

unpack_err:
    return -1;
}

int rs_port_layer_packet_split(struct rs_port_layer_packet *packet,
                               struct rs_port_layer_packet ***split) {
    int len_header = rs_packet_len_header(&packet->super);
    int len = rs_packet_len(&packet->super) - len_header;

    int n_packets =
        ceil((double)len / (double)RS_PORT_LAYER_PACKET_MAX_PAYLOAD);
    int packet_size = ceil((double)len / (double)n_packets);

    *split = calloc(n_packets, sizeof(void*));

    if (n_packets <= 1) {
        **split = packet;
        return 1;
    }

    int buf_len = len + len_header;
    uint8_t *buf = calloc(buf_len, sizeof(uint8_t));

    uint8_t* b = buf;
    int bl = buf_len;
    rs_packet_pack(&packet->super, &b, &bl);


    for (int i = len_header, j = 0; j < n_packets; i += packet_size, j++) {

        int size = packet_size;
        if (size > buf_len - i)
            size = buf_len - i;
        (*split)[j] = calloc(1, sizeof(struct rs_port_layer_packet));
        rs_port_layer_packet_init((*split)[j], j == 0 ? buf : NULL, NULL, buf + i,
                                  size);

        (*split)[j]->command = packet->command;
        (*split)[j]->port = packet->port;
        (*split)[j]->seq = packet->seq;
        (*split)[j]->frag = j;
        (*split)[j]->n_frag = n_packets;
        (*split)[j]->ts = packet->ts;
        (*split)[j]->stats = packet->stats;
        memcpy((*split)[j]->command_payload, packet->command_payload,
               RS_PORT_LAYER_COMMAND_LENGTH);
    }

    return n_packets;
}

static struct rs_packet_vtable vtable = {
    .destroy = &rs_packet_base_destroy,
    .len = &rs_packet_base_len,
    .pack = &rs_packet_base_pack,
    .len_header = &rs_port_layer_packet_len_header,
    .pack_header = &rs_port_layer_packet_pack_header};
