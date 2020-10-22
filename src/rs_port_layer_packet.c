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
    if (*buffer_len < sizeof(rs_port_id_t) + sizeof(rs_port_seq_t)) {
        syslog(LOG_ERR, "pack: buffer too short");
        return;
    }

    for (int i = sizeof(rs_port_id_t) - 1; i >= 0; i--) {
        (**buffer) = (uint8_t)(packet->port >> (8 * i));
        (*buffer)++;
        (*buffer_len)--;
    }
    for (int i = sizeof(rs_port_seq_t) - 1; i >= 0; i--) {
        (**buffer) = (uint8_t)(packet->seq >> (8 * i));
        (*buffer)++;
        (*buffer_len)--;
    }
    if (packet->port == 0) {
        for (int i = 0; i < RS_PORT_LAYER_COMMAND_LENGTH; i++) {
            if ((*buffer_len) == 0) {
                syslog(LOG_ERR, "pack: buffer too short");
                return;
            }
            (**buffer) = packet->command[i];
            (*buffer)++;
            (*buffer_len)--;
        }
    }
}

void rs_port_layer_packet_init(struct rs_port_layer_packet *packet,
                               void *payload_ownership,
                               struct rs_packet *payload_packet,
                               uint8_t *payload_data, int payload_data_len) {
    rs_packet_init(&packet->super, payload_ownership, payload_packet,
                   payload_data, payload_data_len);
    packet->super.vtable = &vtable;
    packet->port = 0;
    packet->seq = 0;
    memset(packet->command, 0, sizeof(packet->command));
}

int rs_port_layer_packet_unpack(struct rs_port_layer_packet *packet,
                                struct rs_packet *from_packet) {

    if (from_packet->payload_data_len <
        sizeof(rs_port_id_t) + sizeof(rs_port_seq_t)) {
        return -1;
    }

    /* Initialize */
    rs_port_layer_packet_init(packet, from_packet->payload_ownership, NULL,
                              from_packet->payload_data,
                              from_packet->payload_data_len);

    /* Set port and seq */
    packet->port = 0;
    for (int i = sizeof(rs_port_id_t) - 1; i >= 0; i--) {
        packet->port += (*packet->super.payload_data) << (8 * i);
        packet->super.payload_data++;
        packet->super.payload_data_len--;
    }
    packet->seq = 0;
    for (int i = sizeof(rs_port_seq_t) - 1; i >= 0; i--) {
        packet->seq += (*packet->super.payload_data) << (8 * i);
        packet->super.payload_data++;
        packet->super.payload_data_len--;
    }

    /* Possibly set command */
    if (packet->port == 0) {
        for (int i = 0; i < RS_PORT_LAYER_COMMAND_LENGTH; i++) {
            if (packet->super.payload_data_len == 0)
                return -1;

            packet->command[i] = (uint8_t)(*packet->super.payload_data);
            packet->super.payload_data++;
            packet->super.payload_data_len--;
        }
    }

    /* Successfully unpacked --> steal data and destroy from_packet */
    from_packet->payload_ownership = NULL;
    rs_packet_destroy(from_packet);

    return 0;
}

static struct rs_packet_vtable vtable = {.destroy = &rs_packet_base_destroy,
                                         .pack = &rs_packet_base_pack,
                                         .pack_header =
                                             &rs_port_layer_packet_pack_header};
