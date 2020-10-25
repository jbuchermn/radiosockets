#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_channel_layer.h"
#include "rs_port_layer.h"
#include "rs_port_layer_packet.h"
#include "rs_server_state.h"
#include "rs_util.h"

void rs_port_layer_init(struct rs_port_layer *layer,
                        struct rs_server_state *server,
                        rs_channel_t default_channel) {
    layer->server = server;

    layer->ports = calloc(1, sizeof(void *));
    layer->ports[0] = calloc(1, sizeof(struct rs_port));
    layer->n_ports = 1;

    /* command port */
    layer->ports[0]->id = 0;
    layer->ports[0]->bound_channel = default_channel;
    layer->ports[0]->status = RS_PORT_OPENED;
    layer->ports[0]->tx_last_seq = 0;
    rs_stats_init(&layer->ports[0]->stats);
}

void rs_port_layer_destroy(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        free(layer->ports[i]);
    }
    free(layer->ports);

    layer->ports = NULL;
}

int _transmit(struct rs_port_layer *layer, struct rs_port_layer_packet *packet,
              struct rs_port *port) {

    struct rs_channel_layer *ch =
        rs_server_channel_layer_for_channel(layer->server, port->bound_channel);
    if (!ch) {
        syslog(LOG_ERR, "Invalid channel");
    }

    /* Publish stats */
    rs_stats_packed_init(&packet->stats, &port->stats);

    int bytes;
    packet->seq = port->tx_last_seq + 1;
    packet->ts = cur_msec();
    if ((bytes = rs_channel_layer_transmit(ch, &packet->super,
                                           port->bound_channel)) > 0) {
        port->tx_last_seq++;
        clock_gettime(CLOCK_REALTIME, &port->tx_last_ts);

        /* Register stats */
        rs_stats_register_tx(&port->stats, bytes);
    }
    return bytes;
}

int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *send_packet, rs_port_id_t port) {

    if (!port) {
        syslog(LOG_ERR, "Wrong API to submit command packets");
        return -1;
    }

    struct rs_port *p = NULL;
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i]->id == port) {
            p = layer->ports[i];
            break;
        }
    }

    if (!p) {
        syslog(LOG_ERR, "Unknown port %d", port);
    }

    struct rs_port_layer_packet packed;
    rs_port_layer_packet_init(&packed, NULL, send_packet, NULL, 0);
    packed.port = port;

    int bytes = _transmit(layer, &packed, p);

    rs_packet_destroy(&packed.super);
    return bytes;
}

static int _receive(struct rs_port_layer *layer, struct rs_packet **packet_ret,
                    rs_port_id_t *port_ret, rs_channel_t channel) {
    struct rs_channel_layer *ch =
        rs_server_channel_layer_for_channel(layer->server, channel);

    if (!ch) {
        syslog(LOG_ERR, "Could not find layer for channel %d", channel);
        return -1;
    }

retry:
    for (;;) {
        struct rs_packet *packet;

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

            struct rs_port *port = NULL;
            for (int i = 0; i < layer->n_ports; i++) {
                if (layer->ports[i]->id == unpacked.port) {
                    port = layer->ports[i];
                }
            }

            /* Handle earlier updates which have been missed (channel switched /
             * port opened) */
            if (!port) {
                /* Could be handled gracefully */
                syslog(LOG_ERR, "Received packet on unknown port");
                goto retry;
            }

            if (port->bound_channel != channel) {
                port->bound_channel = channel;
            }

            /* The same packet is possibly received multiple times */
            if (unpacked.seq == port->rx_last_seq) {
                syslog(LOG_DEBUG, "Duplicate packet");
                rs_packet_destroy(&unpacked.super);
                goto retry;
            }

            rs_stats_register_rx(&port->stats, bytes,
                                 unpacked.seq - port->rx_last_seq - 1,
                                 &unpacked.stats, unpacked.ts);

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

    int haserr = 0;
    for (int i = 0; i < layer->n_ports; i++) {
        /* Possibly multiple receives on same channel, should not be an issue
         * though */
        int res = _receive(layer, packet, port, layer->ports[i]->bound_channel);
        if (!res)
            return 0;
        if (res < 0) {
            haserr = 1;
        }
    }

    if (haserr)
        return -1;
    return RS_PORT_LAYER_EOF;
}

static void _send_command(struct rs_port_layer *layer, const uint8_t *command) {
    static uint8_t dummy[RS_PORT_CMD_DUMMY_SIZE];

    struct rs_port_layer_packet packet;
    rs_port_layer_packet_init(&packet, NULL, NULL, dummy,
                              RS_PORT_CMD_DUMMY_SIZE);
    memcpy(packet.command, command, RS_PORT_LAYER_COMMAND_LENGTH);
    packet.super.payload_ownership = NULL;

    _transmit(layer, &packet, layer->ports[0]);

    rs_packet_destroy(&packet.super);
}

void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received) {

    if (received) {
        if (received->command[0] == RS_PORT_CMD_OPEN) {
            int ack = 0;

            rs_port_id_t port =
                ((uint16_t)received->command[1] << 8) + received->command[2];
            rs_channel_t channel =
                ((uint16_t)received->command[3] << 8) + received->command[4];

            for (int i = 0; i < layer->n_ports; i++) {
                if (layer->ports[i]->id == port) {
                    ack = 1;
                    goto exists;
                }
            }

            struct rs_port *new_port = calloc(1, sizeof(struct rs_port));
            new_port->id = port;
            new_port->bound_channel = channel;
            new_port->status = RS_PORT_OPENED;
            rs_stats_init(&new_port->stats);

            layer->n_ports++;
            layer->ports = realloc(layer->ports, layer->n_ports);
            layer->ports[layer->n_ports - 1] = new_port;
            syslog(LOG_NOTICE, "Successfully opened port %d", port);
            ack = 1;
        exists:;
            if (ack) {
                uint8_t cmd[RS_PORT_LAYER_COMMAND_LENGTH] = {
                    RS_PORT_CMD_ACK_OPEN,
                    /* port id */
                    received->command[1], received->command[2], 0, 0, 0, 0, 0};

                _send_command(layer, cmd);
            }

        } else if (received->command[0] == RS_PORT_CMD_ACK_OPEN) {
            rs_port_id_t port =
                ((uint16_t)received->command[1] << 8) + received->command[2];
            for (int i = 0; i < layer->n_ports; i++) {
                if (layer->ports[i]->id == port &&
                    layer->ports[i]->status == RS_PORT_WAITING_ACK) {
                    layer->ports[i]->status = RS_PORT_OPENED;
                    syslog(LOG_NOTICE, "Successfully opened port %d", port);
                }
            }
        }
    } else {
        /* Ensure all needed channels (and no more are opened) */
        for (int i = 0; i < layer->server->n_channel_layers; i++) {
            rs_channel_layer_close_all_channels(
                layer->server->channel_layers[i]);
            for (int j = 0; j < layer->n_ports; j++) {
                if (rs_channel_layer_owns_channel(
                        layer->server->channel_layers[i],
                        layer->ports[j]->bound_channel))
                    rs_channel_layer_open_channel(
                        layer->server->channel_layers[i],
                        layer->ports[j]->bound_channel);
            }
        }

        /* Open port */
        for (int i = 0; i < layer->n_ports; i++) {
            if (layer->ports[i]->status == RS_PORT_OPENED)
                continue;

            uint8_t cmd_open[RS_PORT_LAYER_COMMAND_LENGTH] = {
                RS_PORT_CMD_OPEN,
                /* port id */
                (uint8_t)(layer->ports[i]->id >> 8),
                (uint8_t)layer->ports[i]->id,
                (uint8_t)(layer->ports[i]->bound_channel >> 8),
                (uint8_t)layer->ports[i]->bound_channel, 0, 0, 0};

            if (layer->ports[i]->status == RS_PORT_INITIAL) {
                syslog(LOG_NOTICE, "Opening port %d", layer->ports[i]->id);
                layer->ports[i]->status = RS_PORT_WAITING_ACK;
                /* Open the port */
                layer->ports[i]->retry_cnt = 0;
                clock_gettime(CLOCK_REALTIME, &layer->ports[i]->last_try);

                _send_command(layer, cmd_open);

            } else if (layer->ports[i]->status == RS_PORT_WAITING_ACK &&
                       layer->ports[i]->retry_cnt < RS_PORT_CMD_RETRY_CNT) {
                /* Retry open */
                struct timespec now;
                clock_gettime(CLOCK_REALTIME, &now);
                long msec = msec_diff(now, layer->ports[i]->last_try);
                if (msec > RS_PORT_CMD_RETRY_MSEC) {

                    _send_command(layer, cmd_open);

                    layer->ports[i]->retry_cnt++;
                    clock_gettime(CLOCK_REALTIME, &layer->ports[i]->last_try);
                }

            } else if (layer->ports[i]->status == RS_PORT_WAITING_ACK &&
                       layer->ports[i]->retry_cnt >= RS_PORT_CMD_RETRY_CNT) {
                /* Open failed */
                syslog(LOG_ERR, "Failed to open port %d", layer->ports[i]->id);
                layer->ports[i]->status = RS_PORT_OPEN_FAILED;
            }
        }
    }
}

int rs_port_layer_open_port(struct rs_port_layer *layer, uint8_t id,
                            rs_port_id_t *opened_id, rs_channel_t channel) {
    rs_port_id_t new_id = ((rs_port_id_t)layer->server->own_id << 8) | id;
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i]->id == new_id) {
            syslog(LOG_ERR, "Port already in use");
            return -1;
        }
    }

    struct rs_port *new_port = calloc(1, sizeof(struct rs_port));
    new_port->id = new_id;
    new_port->bound_channel = channel;
    new_port->status = RS_PORT_INITIAL;
    rs_stats_init(&new_port->stats);

    layer->n_ports++;
    layer->ports = realloc(layer->ports, layer->n_ports);
    layer->ports[layer->n_ports - 1] = new_port;

    *opened_id = new_id;
    return 0;
}

void rs_port_layer_stats_printf(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        printf("-------- %04X --------\n", layer->ports[i]->id);
        rs_stats_printf(&layer->ports[i]->stats);
    }
}
