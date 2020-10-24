#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_channel_layer.h"
#include "rs_port_layer.h"
#include "rs_port_layer_packet.h"
#include "rs_util.h"

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_channel_layer **channel_layers,
                        int n_channel_layers, rs_channel_t default_channel) {
    assert(n_channel_layers > 0);

    layer->n_channel_layers = n_channel_layers;
    layer->channel_layers = channel_layers;

    layer->ports = calloc(1, sizeof(struct rs_port));
    layer->n_ports = 1;

    layer->ports[0].id = 0;
    layer->ports[0].bound_channel = default_channel;

    layer->infos = calloc(layer->n_channel_layers, sizeof(void *));
    for (int i = 0; i < layer->n_channel_layers; i++) {
        layer->infos[i] =
            calloc(rs_channel_layer_ch_n(layer->channel_layers[i]),
                   sizeof(struct rs_port_channel_info));

        for (int j = 0; j < rs_channel_layer_ch_n(layer->channel_layers[i]);
             j++) {
            layer->infos[i][j].id =
                rs_channel_layer_ch(layer->channel_layers[i], j);

            rs_stat_init(&layer->infos[i][j].tx_stat_bits, RS_STAT_AGG_SUM,
                         "TX", "bps", 1000. / RS_STAT_DT_MSEC);
            rs_stat_init(&layer->infos[i][j].tx_stat_packets, RS_STAT_AGG_COUNT,
                         "TX", "pps", 1000. / RS_STAT_DT_MSEC);

            rs_stat_init(&layer->infos[i][j].rx_stat_bits, RS_STAT_AGG_SUM,
                         "RX", "bps", 1000. / RS_STAT_DT_MSEC);
            rs_stat_init(&layer->infos[i][j].rx_stat_packets, RS_STAT_AGG_COUNT,
                         "RX", "pps", 1000. / RS_STAT_DT_MSEC);
            rs_stat_init(&layer->infos[i][j].rx_stat_dt, RS_STAT_AGG_AVG,
                         "RX dt", "ms", 1.);
            rs_stat_init(&layer->infos[i][j].rx_stat_missed, RS_STAT_AGG_AVG,
                         "RX miss", "", 1.);

            rs_stat_init(&layer->infos[i][j].other_rx_stat_bits,
                         RS_STAT_AGG_AVG, "-RX", "bps", 1.);
            rs_stat_init(&layer->infos[i][j].other_rx_stat_dt, RS_STAT_AGG_AVG,
                         "-RX dt", "ms", 1.);
            rs_stat_init(&layer->infos[i][j].other_rx_stat_missed,
                         RS_STAT_AGG_AVG, "-RX miss", "", 1.);
        }
    }
}

static int _retrieve(struct rs_port_layer *layer, rs_port_id_t port_id,
                     struct rs_channel_layer **channel_layer,
                     struct rs_port **port,
                     struct rs_port_channel_info **info) {

    struct rs_port *p = NULL;
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i].id == port_id)
            p = layer->ports + i;
    }
    if (!p) {
        syslog(LOG_ERR, "Could not find port %d", port_id);
        return -1;
    }

    if (port)
        (*port) = p;

    struct rs_channel_layer *ch = NULL;
    int i_ch = 0;
    for (int i = 0; i < layer->n_channel_layers; i++) {
        if (rs_channel_layer_owns_channel(layer->channel_layers[i],
                                          p->bound_channel)) {
            ch = layer->channel_layers[i];
            i_ch = i;
        }
    }

    if (!ch) {
        syslog(LOG_ERR, "Could not find layer for channel %d", port_id);
        return -1;
    }

    if (channel_layer)
        (*channel_layer) = ch;

    struct rs_port_channel_info *in = NULL;
    for (int i = 0; i < rs_channel_layer_ch_n(ch); i++) {
        if (layer->infos[i_ch][i].id == p->bound_channel)
            in = layer->infos[i_ch] + i;
    }

    if (!in) {
        syslog(LOG_ERR, "Could not find info for channel %d", port_id);
        return -1;
    }

    if (info)
        (*info) = in;
    return 0;
}

void rs_port_layer_destroy(struct rs_port_layer *layer) {
    free(layer->ports);
    for (int i = 0; i < layer->n_channel_layers; i++) {
        free(layer->infos[i]);
    }
    free(layer->infos);

    layer->ports = NULL;
    layer->infos = NULL;
}

static int _transmit(struct rs_port_layer *layer,
                     struct rs_port_layer_packet *packet,
                     struct rs_channel_layer *channel_layer,
                     rs_channel_t channel, struct rs_port_channel_info *info) {

    packet->rx_bitrate =
        (uint16_t)(rs_stat_current(&info->rx_stat_bits) / 1000);
    packet->rx_missed =
        (uint16_t)(rs_stat_current(&info->rx_stat_missed) * 10000);
    packet->rx_dt = (uint16_t)(rs_stat_current(&info->rx_stat_dt));

    int bytes;
    if ((bytes = rs_channel_layer_transmit(channel_layer, &packet->super,
                                           channel)) > 0) {
        info->tx_last_seq++;
        clock_gettime(CLOCK_REALTIME, &info->tx_last_ts);

        rs_stat_register(&info->tx_stat_packets, 1.0);
        rs_stat_register(&info->tx_stat_bits, 8 * bytes);
        return bytes;
    }

    return -1;
}

int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *send_packet, rs_port_id_t port) {

    if (!port) {
        syslog(LOG_ERR, "Wrong API to submit command packets");
        return -1;
    }

    struct rs_port *p = NULL;
    struct rs_channel_layer *ch = NULL;
    struct rs_port_channel_info *info = NULL;
    if (_retrieve(layer, port, &ch, &p, &info))
        return -1;

    struct rs_port_layer_packet packet;
    rs_port_layer_packet_init(&packet, NULL, send_packet, NULL, 0);
    packet.command[0] = RS_PORT_CMD_HEARTBEAT;
    packet.seq = info->tx_last_seq + 1;

    int res = _transmit(layer, &packet, ch, p->bound_channel, info);

    rs_packet_destroy(&packet.super);
    return res;
}

static int _receive(struct rs_port_layer *layer, struct rs_packet **packet_ret,
                    rs_port_id_t *port_ret, rs_channel_t chan) {
    struct rs_channel_layer *ch = NULL;
    int i_ch;
    for (int i = 0; i < layer->n_channel_layers; i++) {
        if (rs_channel_layer_owns_channel(layer->channel_layers[i], chan)) {
            ch = layer->channel_layers[i];
            i_ch = i;
        }
    }

    if (!ch) {
        syslog(LOG_ERR, "Could not find layer for channel %d", chan);
        return -1;
    }

    struct rs_port_channel_info *info = NULL;
    for (int i = 0; i < rs_channel_layer_ch_n(ch); i++) {
        if (layer->infos[i_ch][i].id == chan)
            info = layer->infos[i_ch] + i;
    }
    if (!info) {
        syslog(LOG_ERR, "Could not info layer for channel %d", chan);
        return -1;
    }

retry:
    for (;;) {
        struct rs_packet *packet;
        rs_channel_t channel = chan;

        int bytes;
        struct rs_port_layer_packet unpacked;
        switch (rs_channel_layer_receive(ch, &packet, &channel)) {
        case 0:
            bytes = packet->payload_data_len;

            if (rs_port_layer_packet_unpack(&unpacked, packet)) {
                /* packed that could not be unpacked */
                syslog(LOG_ERR, "Could not unpack at port layer");
                return -1;
            }
            /* At this point, packet is invalid, we only have unpacked */

            /* The same packet is possibly received multiple times */
            if (unpacked.seq == info->rx_last_seq) {
                syslog(LOG_DEBUG, "Duplicate packet");
                rs_packet_destroy(&unpacked.super);
                goto retry;
            }

            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            uint64_t now_nsec =
                now.tv_sec * (uint64_t)1000000000L + now.tv_nsec;
            double dt_msec = (now_nsec - unpacked.ts_sent) / 1000000L;

            rs_stat_register(&info->rx_stat_packets, 1.0);
            rs_stat_register(&info->rx_stat_bits, 8 * bytes);
            rs_stat_register(&info->rx_stat_dt, dt_msec);
            for (int i = 0; i < unpacked.seq - info->rx_last_seq - 1; i++)
                rs_stat_register(&info->rx_stat_missed, 1.0);
            rs_stat_register(&info->rx_stat_missed, 0.0);

            info->rx_last_ts = now;
            info->rx_last_seq = unpacked.seq;

            rs_stat_register(&info->other_rx_stat_bits,
                             1000 * (double)unpacked.rx_bitrate);
            rs_stat_register(&info->other_rx_stat_missed,
                             0.0001 * (double)unpacked.rx_missed);
            rs_stat_register(&info->other_rx_stat_dt,
                             0.0001 * (double)unpacked.rx_dt);

            if (unpacked.port == 0) {
                /* Received a command packet */
                rs_port_layer_main(layer, &unpacked);
                rs_packet_destroy(&unpacked.super);
                goto retry;
            }

            *packet_ret = calloc(1, sizeof(struct rs_packet));
            rs_packet_init(*packet_ret, unpacked.super.payload_ownership,
                           unpacked.super.payload_packet,
                           unpacked.super.payload_data,
                           unpacked.super.payload_data_len);
            *port_ret = unpacked.port;

            rs_packet_destroy(&unpacked.super);

            return 0;

            break;
        case RS_CHANNEL_LAYER_EOF:
            /* No more packets */
            return RS_PORT_LAYER_EOF;
        case RS_CHANNEL_LAYER_IRR:
            /* Received a packet we do not care about */
            goto retry;
            break;
        case RS_CHANNEL_LAYER_BADFCS:
            /* Received a packet with bad fcs */
            goto retry;
            break;
        default:
            /* Exception */
            return -1;
        }
    }

    return 0;
}

int rs_port_layer_receive(struct rs_port_layer *layer,
                          struct rs_packet **packet, rs_port_id_t *port) {
    /*
     * For now, only listen on command channel
     */
    return _receive(layer, packet, port, layer->ports[0].bound_channel);
}

void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received) {

    struct rs_channel_layer *ch = NULL;
    struct rs_port_channel_info *info = NULL;
    if (_retrieve(layer, layer->ports[0].id, &ch, NULL, &info))
        return;
    if (received) {
        if (received->command[0] == RS_PORT_CMD_HEARTBEAT) {
            if (received->super.payload_data_len != RS_PORT_CMD_DUMMY_SIZE) {
                syslog(LOG_ERR, "Received heartbeat of wrong size");
            }
            /* Okay */
        }
    } else {
        /*
         * For now only a heartbeat through command port
         */

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        long msec = msec_diff(now, info->tx_last_ts);
        if (msec >= RS_PORT_CMD_HEARTBEAT_MSEC) {
            uint8_t *dummy = calloc(RS_PORT_CMD_DUMMY_SIZE, sizeof(uint8_t));
            memset(dummy, 0xDD, RS_PORT_CMD_DUMMY_SIZE);

            struct rs_port_layer_packet packet;
            rs_port_layer_packet_init(&packet, dummy, NULL, dummy,
                                      RS_PORT_CMD_DUMMY_SIZE);
            packet.command[0] = RS_PORT_CMD_HEARTBEAT;
            packet.seq = info->tx_last_seq + 1;

            _transmit(layer, &packet, ch, layer->ports[0].bound_channel, info);

            rs_packet_destroy(&packet.super);
        }
    }
}

int rs_port_layer_get_channel_info(struct rs_port_layer *layer,
                                   rs_port_id_t port,
                                   struct rs_port_channel_info **info) {
    return _retrieve(layer, port, NULL, NULL, info);
}
