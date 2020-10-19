#ifndef RS_PACKET_H
#define RS_PACKET_H

#define RS_PACKET_ALLOCATE_HEADER 1

struct rs_packet {
    /* Does not take ownership of memory */
    uint8_t* begin;
    uint8_t* begin_payload;
    uint16_t total_size;
    uint16_t max_size;
};

void rs_packet_init(struct rs_packet* packet, uint8_t* buf, uint16_t buf_size);
void rs_packet_destroy(struct rs_packet* packet);

#endif
