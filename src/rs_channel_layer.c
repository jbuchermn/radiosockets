#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "rs_channel_layer.h"
#include "rs_channel_layer_packet.h"
#include "rs_util.h"

void rs_channel_layer_init(struct rs_channel_layer *layer,
                           struct rs_server_state *server, uint8_t ch_base,
                           struct rs_channel_layer_vtable *vtable) {
    layer->vtable = vtable;
    layer->server = server;
    layer->ch_base = ch_base;
    layer->channels =
        calloc(rs_channel_layer_ch_n(layer), sizeof(struct rs_channel_info));
    for (int i = 0; i < rs_channel_layer_ch_n(layer); i++) {
        layer->channels[i].id = rs_channel_layer_ch(layer, i);
        layer->channels[i].is_in_use = 0;
        layer->channels[i].tx_last_seq = 0;
        rs_stats_init(&layer->channels[i].stats);
        rs_stat_init(&layer->channels[i].tx_stat_dt, RS_STAT_AGG_SUM, "TX",
                     "s", 1.);
    }
}
void rs_channel_layer_base_destroy(struct rs_channel_layer *layer) {
    free(layer->channels);
    layer->channels = NULL;
}

int rs_channel_layer_owns_channel(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    if ((uint8_t)(channel >> 12) != layer->ch_base)
        return 0;
    if (rs_channel_layer_extract(layer, channel) >=
        rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "owns_channel: Encountered invalid channel");
        return 0;
    }

    return 1;
}

rs_channel_t rs_channel_layer_ch(struct rs_channel_layer *layer, int i) {
    if (i >= rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "ch: Constructing invalid channel");
    }
    return (layer->ch_base << 12) + i;
}

uint16_t rs_channel_layer_extract(struct rs_channel_layer *layer,
                                  rs_channel_t channel) {
    uint16_t res = (uint16_t)(0x0FFF & channel);
    if (res >= rs_channel_layer_ch_n(layer)) {
        syslog(LOG_ERR, "extract: Encountered invalid channel: %40x / %d",
               channel, res);
    }
    return res;
}

static int _transmit(struct rs_channel_layer *layer,
                     struct rs_channel_layer_packet *packet,
                     struct rs_channel_info *info) {

    /* Publish stats */
    rs_stats_packed_init(&packet->stats, &info->stats);

    info->is_in_use = 1;

    packet->channel = info->id;
    packet->seq = info->tx_last_seq + 1;

    struct timespec before_tx;
    clock_gettime(CLOCK_REALTIME, &before_tx);
    int res = layer->vtable->_transmit(layer, &packet->super, info->id);
    if (res > 0) {
        info->tx_last_seq++;
        clock_gettime(CLOCK_REALTIME, &info->tx_last_ts);

        uint64_t nsec =
            1000000000LL * (info->tx_last_ts.tv_sec - before_tx.tv_sec) +
            (info->tx_last_ts.tv_nsec - before_tx.tv_nsec);
        rs_stat_register(&info->tx_stat_dt, nsec / 1000000000.0);

        /* Register stats */
        rs_stats_register_tx(&info->stats, res);
    } else {
        rs_stat_register(&info->stats.tx_stat_errors, 1.0);
    }

    return res;
}

int rs_channel_layer_transmit(struct rs_channel_layer *layer,
                              struct rs_packet *packet, rs_channel_t channel) {

    if (!rs_channel_layer_owns_channel(layer, channel))
        return -1;
    struct rs_channel_info *info =
        &layer->channels[rs_channel_layer_extract(layer, channel)];

    struct rs_channel_layer_packet packed;
    rs_channel_layer_packet_init(&packed, NULL, packet, NULL, 0);
    packed.command = 0;

    int res = _transmit(layer, &packed, info);
    rs_packet_destroy(&packed.super);

    return res;
}

int rs_channel_layer_receive(struct rs_channel_layer *layer,
                             struct rs_packet **packet, rs_channel_t *channel) {

    struct rs_channel_layer_packet *unpacked;
    int res = layer->vtable->_receive(layer, &unpacked, *channel);
    if (res)
        return res;

    if (!rs_channel_layer_owns_channel(layer, unpacked->channel)) {
        syslog(LOG_DEBUG, "Received packet on channel without ownership");
        rs_packet_destroy(&unpacked->super);
        free(unpacked);
        return RS_CHANNEL_LAYER_IRR;
    }

    struct rs_channel_info *info =
        &layer->channels[rs_channel_layer_extract(layer, *channel)];

    info->is_in_use = 1;
    rs_stats_register_rx(&info->stats, unpacked->super.payload_data_len,
                         unpacked->seq - info->rx_last_seq - 1,
                         &unpacked->stats);
    info->rx_last_seq = unpacked->seq;

    if (unpacked->command) {
        if (unpacked->command == RS_CHANNEL_CMD_HEARTBEAT) {
            /* okay */
        } else {
            syslog(LOG_ERR, "Unknown channel layer command %02x",
                   unpacked->command);
        }
        rs_packet_destroy(&unpacked->super);
        free(unpacked);
        return RS_CHANNEL_LAYER_IRR;
    }

    /* Move ownership to base class struct */
    *packet = calloc(1, sizeof(struct rs_packet));
    rs_packet_init(*packet, unpacked->super.payload_ownership,
                   unpacked->super.payload_packet, unpacked->super.payload_data,
                   unpacked->super.payload_data_len);
    *channel = unpacked->channel;

    unpacked->super.payload_ownership = NULL;
    rs_packet_destroy(&unpacked->super);
    free(unpacked);

    return 0;
}

void rs_channel_layer_main(struct rs_channel_layer *layer) {
    static uint8_t dummy[RS_CHANNEL_CMD_DUMMY_SIZE];

    /*
     * heartbeat through used channels
     */
    for (int j = 0; j < rs_channel_layer_ch_n(layer); j++) {
        if (!layer->channels[j].is_in_use)
            continue;

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        long msec = msec_diff(now, layer->channels[j].tx_last_ts);
        if (msec >= RS_CHANNEL_CMD_HEARTBEAT_MSEC) {
            struct rs_channel_layer_packet packet;
            rs_channel_layer_packet_init(&packet, NULL, NULL, dummy,
                                         RS_CHANNEL_CMD_DUMMY_SIZE);

            packet.command = RS_CHANNEL_CMD_HEARTBEAT;
            _transmit(layer, &packet, &layer->channels[j]);
            rs_packet_destroy(&packet.super);
        }
    }
}

void rs_channel_layer_close_all_channels(struct rs_channel_layer *layer) {
    for (int i = 0; i < rs_channel_layer_ch_n(layer); i++) {
        layer->channels[i].is_in_use = 0;
    }
}
void rs_channel_layer_open_channel(struct rs_channel_layer *layer,
                                   rs_channel_t channel) {
    for (int i = 0; i < rs_channel_layer_ch_n(layer); i++) {
        if (layer->channels[i].id == channel)
            layer->channels[i].is_in_use = 1;
    }
}

void rs_channel_layer_stats_printf(struct rs_channel_layer *layer) {
    for (int i = 0; i < rs_channel_layer_ch_n(layer); i++) {
        if (layer->channels[i].is_in_use) {
            printf("-------- %04X --------\n", layer->channels[i].id);
            rs_stats_printf(&layer->channels[i].stats);
        }
    }
}
