#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rs_packet.h"

void rs_packet_init(struct rs_packet* packet, uint8_t* buf, uint16_t buf_size){
    assert(buf_size >= RS_PACKET_ALLOCATE_HEADER + 1);
    packet->begin = buf;
    packet->max_size = buf_size;
    packet->total_size = RS_PACKET_ALLOCATE_HEADER + 1;
}

void rs_packet_destroy(struct rs_packet* packet){}
