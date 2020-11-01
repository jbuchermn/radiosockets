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
    return sizeof(rs_port_id_t) + sizeof(uint8_t) + sizeof(uint32_t) +
           sizeof(rs_port_layer_seq_t) + 3 * sizeof(rs_port_layer_frag_t) +
           rs_stats_packed_len(&packet->stats) +
           (packet->command != 0
                ? RS_PORT_LAYER_COMMAND_LENGTH * sizeof(uint8_t)
                : 0);
}

void rs_port_layer_packet_pack_header(struct rs_packet *super, uint8_t **buffer,
                                      int *buffer_len) {
    struct rs_port_layer_packet *packet = rs_cast(rs_port_layer_packet, super);

    PACK(buffer, buffer_len, uint8_t, packet->command);
    PACK(buffer, buffer_len, rs_port_id_t, packet->port);
    PACK(buffer, buffer_len, rs_port_layer_seq_t, packet->seq);
    PACK(buffer, buffer_len, uint32_t, packet->payload_len);
    PACK(buffer, buffer_len, rs_port_layer_frag_t, packet->frag);
    PACK(buffer, buffer_len, rs_port_layer_frag_t, packet->n_frag_decoded);
    PACK(buffer, buffer_len, rs_port_layer_frag_t, packet->n_frag_encoded);

    if (rs_stats_packed_pack(&packet->stats, buffer, buffer_len))
        goto pack_err;

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
    packet->n_frag_decoded = 1;
    packet->n_frag_encoded = 1;
    packet->payload_len = payload_data_len;
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
           uint32_t, &packet->payload_len);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_frag_t, &packet->frag);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_frag_t, &packet->n_frag_decoded);
    UNPACK(&packet->super.payload_data, &packet->super.payload_data_len,
           rs_port_layer_frag_t, &packet->n_frag_encoded);

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
                               struct rs_port *port,
                               struct rs_port_layer_packet ***split) {
    int len_header = rs_packet_len_header(&packet->super);
    int len = rs_packet_len(&packet->super) - len_header;

    if (len <= RS_PORT_LAYER_PACKET_MAX_PAYLOAD) {
        *split = calloc(1, sizeof(void *));
        **split = packet;
        return 1;
    }

    /* TODO: Adjust fec */
    assert(port->fec_k * RS_PORT_LAYER_PACKET_MAX_PAYLOAD >= len);
    int packet_len = ceil((double)len / (double)port->fec_k);

    /* Pack into buf with primary block 0 starting at buf + len_header also
     * allocating memory for secondary blocks starting at buf + len_header +
     * k*packet_len */
    int buf_len = len_header + packet_len * port->fec_m;
    uint8_t *buf = calloc(buf_len, sizeof(uint8_t));

    uint8_t *b = buf;
    int bl = buf_len;
    rs_packet_pack(&packet->super, &b, &bl);

    /* Encode secondary blocks */
    uint8_t **primary_blocks = calloc(port->fec_k, sizeof(void *));
    uint8_t **secondary_blocks =
        calloc(port->fec_m - port->fec_k, sizeof(void *));
    unsigned int *block_nums =
        calloc(port->fec_m - port->fec_k, sizeof(unsigned int));

    for (int i = 0; i < port->fec_k; i++) {
        primary_blocks[i] = buf + len_header + packet_len * i;
    }
    for (int i = port->fec_k, j = 0; i < port->fec_m; i++, j++) {
        secondary_blocks[j] = buf + len_header + packet_len * i;
        block_nums[j] = i;
    }
    fec_encode(port->fec, (const uint8_t **)primary_blocks, secondary_blocks,
               block_nums, port->fec_m - port->fec_k, packet_len);
    free(primary_blocks);
    free(secondary_blocks);
    free(block_nums);

    /* Allocate packet array */
    *split = calloc(port->fec_m, sizeof(void *));

    /* Fill packet array */
    for (int j = 0; j < port->fec_m; j++) {
        (*split)[j] = calloc(1, sizeof(struct rs_port_layer_packet));
        rs_port_layer_packet_init((*split)[j], j == 0 ? buf : NULL, NULL,
                                  buf + len_header + (packet_len * j),
                                  packet_len);

        (*split)[j]->command = packet->command;
        (*split)[j]->port = packet->port;
        (*split)[j]->payload_len = len;
        (*split)[j]->seq = packet->seq;
        (*split)[j]->frag = j;
        (*split)[j]->n_frag_decoded = port->fec_k;
        (*split)[j]->n_frag_encoded = port->fec_m;
        (*split)[j]->stats = packet->stats;
        memcpy((*split)[j]->command_payload, packet->command_payload,
               RS_PORT_LAYER_COMMAND_LENGTH);
    }

    return port->fec_m;
}

int rs_port_layer_packet_join(struct rs_port_layer_packet *joined,
                              struct rs_port *port,
                              struct rs_port_layer_packet **split,
                              int n_split) {

    /* No packed packets here */
    for (int i = 0; i < n_split; i++) {
        assert(split[i]->super.payload_data);
    }

    /* Infer / read FEC parameters */
    int packet_len = split[0]->super.payload_data_len;
    rs_port_setup_fec(port, split[0]->n_frag_decoded, split[0]->n_frag_encoded);

    /* Allocate packet index array */
    unsigned int *block_nums = calloc(port->fec_k, sizeof(unsigned int));
    for (int i = 0; i < port->fec_k; i++)
        block_nums[i] = port->fec_m + 1;

    /* Allocate output buffer and place received primary and secondary packets
     * inside */
    uint8_t *buf = calloc(packet_len * port->fec_k, sizeof(uint8_t));
    for (int i = 0; i < n_split; i++) {
        if (split[i]->frag < port->fec_k) {
            memcpy(buf + split[i]->frag * packet_len,
                   split[i]->super.payload_data, packet_len);
            block_nums[split[i]->frag] = split[i]->frag;
        }
    }
    for (int i = 0; i < n_split; i++) {
        if (split[i]->frag >= port->fec_k) {
            int index = -1;
            for (int k = 0; k < port->fec_k; k++) {
                if (block_nums[k] > port->fec_k) {
                    index = k;
                    break;
                }
            }
            if (index >= port->fec_k)
                break;

            memcpy(buf + index * packet_len, split[i]->super.payload_data,
                   packet_len);
            block_nums[index] = split[i]->frag;
        }
    }

    /* Decode */
    uint8_t **input = calloc(port->fec_k, sizeof(void *));

    uint8_t *output_buf = calloc(port->fec_k * packet_len, sizeof(uint8_t));
    uint8_t **output = calloc(port->fec_k, sizeof(void *));

    for (int i = 0; i < port->fec_k; i++)
        input[i] = buf + i * packet_len;
    for (int i = 0; i < port->fec_k; i++)
        output[i] = output_buf + i * packet_len;

    fec_decode(port->fec, (const uint8_t **)input, output, block_nums,
               packet_len);
    free(output);
    free(input);

    /* Write output back to buf */
    for (int i = 0, j = 0; i < port->fec_k; i++) {
        if (block_nums[i] >= port->fec_k) {
            memcpy(buf + i * packet_len, output_buf + j * packet_len,
                   packet_len * sizeof(uint8_t));
            j++;
        }
    }
    free(output_buf);
    free(block_nums);

    /* Setup returned packet */
    rs_port_layer_packet_init(joined, buf, NULL, buf, split[0]->payload_len);
    joined->command = split[0]->command;
    joined->port = split[0]->port;
    joined->seq = split[0]->seq;
    joined->frag = 0;
    joined->n_frag_decoded = 1;
    joined->n_frag_encoded = 1;
    joined->stats = split[0]->stats;
    memcpy(joined->command_payload, split[0]->command_payload,
           RS_PORT_LAYER_COMMAND_LENGTH);

    return 0;
}

static struct rs_packet_vtable vtable = {
    .destroy = &rs_packet_base_destroy,
    .len = &rs_packet_base_len,
    .pack = &rs_packet_base_pack,
    .len_header = &rs_port_layer_packet_len_header,
    .pack_header = &rs_port_layer_packet_pack_header};
