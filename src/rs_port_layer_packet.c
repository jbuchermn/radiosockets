#include <errno.h>
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

void rs_port_layer_packet_pack_header(struct rs_packet *super, uint8_t **buffer,
                                      int *buffer_len) {
    struct rs_port_layer_packet *packet = rs_cast(rs_port_layer_packet, super);

    /* command */
    PACK(buffer, buffer_len, uint8_t, packet->command);
    PACK(buffer, buffer_len, rs_port_id_t, packet->port);
    PACK(buffer, buffer_len, rs_port_layer_seq_t, packet->seq);
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

static struct rs_packet_vtable vtable = {.destroy = &rs_packet_base_destroy,
                                         .pack = &rs_packet_base_pack,
                                         .pack_header =
                                             &rs_port_layer_packet_pack_header};
