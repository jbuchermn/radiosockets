#ifndef RS_PACKET_H
#define RS_PACKET_H

#include <stdint.h>

struct rs_packet_vtable;

struct rs_packet {
    /* Only one should be nonnull */
    struct rs_packet *payload_packet;

    int payload_owner;
    uint8_t *payload_data;
    int payload_data_len;

    struct rs_packet_vtable *vtable;
};

void rs_packet_init(struct rs_packet *packet, struct rs_packet *payload_packet,
                    uint8_t *payload_data, int payload_data_len);

struct rs_packet_vtable {
    void (*destroy)(struct rs_packet *packet);
    void (*pack)(struct rs_packet *packet, uint8_t **buffer, int *buffer_len);
    void (*pack_header)(struct rs_packet *packet, uint8_t **buffer,
                        int *buffer_len);
};

void rs_packet_base_destroy(struct rs_packet *packet);
void rs_packet_base_pack(struct rs_packet *packet, uint8_t **buffer,
                         int *buffer_len);
void rs_packet_base_pack_header(struct rs_packet *packet, uint8_t **buffer,
                                int *buffer_len);

static inline void rs_packet_destroy(struct rs_packet *packet) {
    (packet->vtable->destroy)(packet);
}

static inline void rs_packet_pack(struct rs_packet *packet, uint8_t **buffer,
                                  int *buffer_len) {
    (packet->vtable->pack)(packet, buffer, buffer_len);
}

static inline void rs_packet_pack_header(struct rs_packet *packet,
                                         uint8_t **buffer, int *buffer_len) {
    (packet->vtable->pack_header)(packet, buffer, buffer_len);
}

#endif
