#ifndef RS_PACKET_H
#define RS_PACKET_H

#include <stdint.h>

struct rs_packet_vtable;

struct rs_packet {
    /*
     * Either payload_packet or payload_data and payload_data_len can be set
     * (not both)
     *
     * nonnull payload_ownership indicates that memory will be freed on destroy
     * (either a packed packet or data, in the latter case payload_ownership
     * should point to the allocated memory)
     *
     * During unpacking payload_data may advance past payload_ownership, freeing
     * payload_data will then result in a segfault
     */
    void *payload_ownership;
    struct rs_packet *payload_packet;

    uint8_t *payload_data;
    int payload_data_len;

    struct rs_packet_vtable *vtable;
};

void rs_packet_init(struct rs_packet *packet, void *payload_ownerhip,
                    struct rs_packet *payload_packet, uint8_t *payload_data,
                    int payload_data_len);

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
