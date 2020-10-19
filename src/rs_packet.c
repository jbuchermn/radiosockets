#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>

#include "rs_packet.h"

static struct rs_packet_vtable vtable;

void rs_packet_init(struct rs_packet* packet, struct rs_packet* payload_packet, uint8_t* payload_data, int payload_data_len){
    packet->payload_packet = payload_packet;
    packet->payload_data = payload_data;
    packet->payload_data_len = payload_data_len;
    packet->vtable = &vtable;
}

static void rs_packet_base_pack(struct rs_packet* packet, uint8_t** buffer, int* buffer_len){
    rs_packet_pack_header(packet, buffer, buffer_len);

    if(packet->payload_packet){
        rs_packet_pack(packet->payload_packet, buffer, buffer_len);
    }else{
        int len = packet->payload_data_len;
        if(packet->payload_data_len > *buffer_len){
            len = *buffer_len;
            syslog(LOG_ERR, "pack: buffer too short");
        }
        memcpy(*buffer, packet->payload_data, len);
        (*buffer) += len;
        (*buffer_len) -= len;
    }
}

static void rs_packet_base_pack_header(struct rs_packet* packet, uint8_t** buffer, int* buffer_len){}

static void rs_packet_base_destroy(struct rs_packet* packet){
    free(packet->payload_data);
}

static struct rs_packet_vtable vtable = {
    .pack = rs_packet_base_pack,
    .pack_header = rs_packet_base_pack_header,
    .destroy = rs_packet_base_destroy,
};
