#include <errno.h>
#include <libconfig.h>
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
                        struct rs_server_state *server) {
    layer->server = server;

    layer->ports = NULL;
    layer->n_ports = 0;

    /* ports */
    config_setting_t *c = config_lookup(&server->config, "ports");
    int n_ports_conf = c ? config_setting_length(c) : 0;
    for (int i = 0; i < n_ports_conf; i++) {
        char p[100];

        int id;
        sprintf(p, "ports.[%d].id", i);
        config_lookup_int(&server->config, p, &id);

        int bound_channel;
        sprintf(p, "ports.[%d].bound_channel", i);
        config_lookup_int(&server->config, p, &bound_channel);

        int owner = server->other_id;
        sprintf(p, "ports.[%d].owner", i);
        config_lookup_int(&server->config, p, &owner);

        rs_port_layer_create_port(layer, id, bound_channel,
                                  owner == server->own_id);
    }
}

void rs_port_layer_create_port(struct rs_port_layer *layer, rs_port_id_t port,
                               rs_channel_t bound_to, int owner) {
    if (port) {
        for (int i = 0; i < layer->n_ports; i++) {
            if (layer->ports[i]->id == port) {
                syslog(LOG_ERR, "Port already in use");
                return;
            }
        }
    }

    struct rs_port *new_port = calloc(1, sizeof(struct rs_port));
    new_port->id = port;
    new_port->bound_channel = bound_to;
    new_port->tx_last_seq = 0;
    new_port->owner = owner;
    rs_stats_init(&new_port->stats);

    layer->n_ports++;
    layer->ports = realloc(layer->ports, layer->n_ports * sizeof(void *));
    layer->ports[layer->n_ports - 1] = new_port;
}

void rs_port_layer_destroy(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        free(layer->ports[i]);
    }
    free(layer->ports);

    layer->ports = NULL;
}

static int _transmit_fragmented(struct rs_port_layer *layer,
                                struct rs_port_layer_packet *packet,
                                struct rs_port *port,
                                struct rs_channel_layer *channel_layer) {

    /* Split packet if necessary */
    struct rs_port_layer_packet **fragments;

    int n_fragments = rs_port_layer_packet_split(packet, &fragments);

    int total_bytes = 0;
    int bytes;
    for (int i = 0; i < n_fragments; i++) {
        if ((bytes =
                 rs_channel_layer_transmit(channel_layer, &fragments[i]->super,
                                           port->bound_channel)) > 0) {
            total_bytes += bytes;
        } else {
            total_bytes = -1;
            goto cleanup;
        }
    }

cleanup:
    for (int i = 0; i < n_fragments; i++) {
        if (fragments[i] != packet) {
            rs_packet_destroy(&fragments[i]->super);
            free(fragments[i]);
        }
    }

    free(fragments);

    return total_bytes;
}

static int _transmit(struct rs_port_layer *layer,
                     struct rs_port_layer_packet *packet,
                     struct rs_port *port) {

    struct rs_channel_layer *ch =
        rs_server_channel_layer_for_channel(layer->server, port->bound_channel);
    if (!ch) {
        syslog(LOG_ERR, "Invalid channel: %d", port->bound_channel);
    }

    /* Publish stats */
    rs_stats_packed_init(&packet->stats, &port->stats);

    int bytes;
    packet->port = port->id;
    packet->seq = port->tx_last_seq + 1;
    packet->ts = cur_msec();

    if ((bytes = _transmit_fragmented(layer, packet, port, ch)) > 0) {
        port->tx_last_seq++;
        clock_gettime(CLOCK_REALTIME, &port->tx_last_ts);

        /* Register stats */
        rs_stats_register_tx(&port->stats, bytes);
    }
    return bytes;
}

int rs_port_layer_transmit(struct rs_port_layer *layer,
                           struct rs_packet *send_packet, rs_port_id_t port) {

    struct rs_port *p = NULL;
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i]->id == port) {
            p = layer->ports[i];
            break;
        }
    }

    if (!p) {
        syslog(LOG_ERR, "Unknown port: %d", port);
        return 0;
    }

    struct rs_port_layer_packet packed;
    rs_port_layer_packet_init(&packed, NULL, send_packet, NULL, 0);

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

            if (!port) {
                syslog(LOG_ERR, "Received packet on unknown port");
                goto retry;
            }

            /*
             * Handle earlier updates which have been missed (channel switched)
             */
            if (port->bound_channel != channel && !port->owner) {
                syslog(LOG_ERR, "Appears the port has switched channels: %d",
                       channel);
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
            port->rx_last_seq = unpacked.seq;

            if (unpacked.command) {
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

static void _send_command(struct rs_port_layer *layer, struct rs_port *port,
                          uint8_t command, const uint8_t *command_payload) {
    static uint8_t dummy[RS_PORT_CMD_DUMMY_SIZE];

    struct rs_port_layer_packet packet;
    rs_port_layer_packet_init(&packet, NULL, NULL, dummy,
                              RS_PORT_CMD_DUMMY_SIZE);
    memcpy(packet.command_payload, command_payload,
           RS_PORT_LAYER_COMMAND_LENGTH);
    packet.command = command;
    packet.super.payload_ownership = NULL;

    _transmit(layer, &packet, port);

    rs_packet_destroy(&packet.super);
}

void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received) {

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (received) {
        if (received->command == RS_PORT_CMD_HEARTBEAT) {
            /* heartbeat */

        } else if (received->command == RS_PORT_CMD_SWITCH_CHANNEL) {
            /* switch channel */

            rs_channel_t channel = (received->command_payload[0] << 8) +
                                   received->command_payload[1];
            int n_broadcasts = received->command_payload[2];

            struct rs_port *p = NULL;
            for (int i = 0; i < layer->n_ports; i++) {
                if (layer->ports[i]->id == received->port) {
                    p = layer->ports[i];
                    break;
                }
            }
            if (p) {
                syslog(LOG_NOTICE, "Switch channel announced by owner: %d",
                       channel);
                p->cmd_switch_state.state = RS_PORT_CMD_SWITCH_FOLLOWING;

                p->cmd_switch_state.at = now;
                timespec_plus_ms(
                    &p->cmd_switch_state.at,
                    (RS_PORT_CMD_SWITCH_N_BROADCAST - n_broadcasts) *
                        RS_PORT_CMD_SWITCH_DT_BROADCAST_MSEC);
                p->cmd_switch_state.new_channel = channel;
            }

        } else if (received->command == RS_PORT_CMD_REQUEST_SWITCH_CHANNEL) {
            /* request switch channel */
            rs_channel_t channel = (received->command_payload[0] << 8) +
                                   received->command_payload[1];
            struct rs_port *p = NULL;
            for (int i = 0; i < layer->n_ports; i++) {
                if (layer->ports[i]->id == received->port) {
                    p = layer->ports[i];
                    break;
                }
            }
            if (p) {
                syslog(LOG_NOTICE, "Switch channel requested: %d", channel);
                if (p->cmd_switch_state.state == RS_PORT_CMD_SWITCH_NONE) {
                    rs_port_layer_switch_channel(layer, received->port,
                                                 channel);
                }
            }
        }
    } else {
        /* ensure all needed channels (and no more are opened) */
        for (int i = 0; i < layer->server->n_channel_layers; i++) {
            rs_channel_layer_close_all_channels(
                layer->server->channel_layers[i]);
            for (int j = 0; j < layer->n_ports; j++) {
                if (rs_channel_layer_owns_channel(
                        layer->server->channel_layers[i],
                        layer->ports[j]->bound_channel)) {
                    rs_channel_layer_open_channel(
                        layer->server->channel_layers[i],
                        layer->ports[j]->bound_channel);
                }
            }
        }

        /* Switch channel */
        for (int i = 0; i < layer->n_ports; i++) {
            if (layer->ports[i]->cmd_switch_state.state ==
                RS_PORT_CMD_SWITCH_NONE)
                continue;

            if ((layer->ports[i]->cmd_switch_state.state ==
                     RS_PORT_CMD_SWITCH_OWNING ||
                 layer->ports[i]->cmd_switch_state.state ==
                     RS_PORT_CMD_SWITCH_REQUESTING) &&

                layer->ports[i]->cmd_switch_state.n_broadcasts <
                    RS_PORT_CMD_SWITCH_N_BROADCAST &&

                msec_diff(now, layer->ports[i]->cmd_switch_state.begin) >
                    layer->ports[i]->cmd_switch_state.n_broadcasts *
                        RS_PORT_CMD_SWITCH_DT_BROADCAST_MSEC) {
                /* broadcast switch or request */

                assert(sizeof(rs_port_id_t) == 2 && sizeof(rs_channel_t) == 2);
                uint8_t cmd[RS_PORT_LAYER_COMMAND_LENGTH] = {
                    /* new channel id */
                    (uint8_t)(layer->ports[i]->cmd_switch_state.new_channel >>
                              8),
                    (uint8_t)layer->ports[i]->cmd_switch_state.new_channel,
                    /* broadcast number */
                    layer->ports[i]->cmd_switch_state.n_broadcasts, 0, 0, 0, 0,
                    0};

                layer->ports[i]->cmd_switch_state.n_broadcasts++;
                _send_command(layer, layer->ports[i],
                              layer->ports[i]->cmd_switch_state.state ==
                                      RS_PORT_CMD_SWITCH_OWNING
                                  ? RS_PORT_CMD_SWITCH_CHANNEL
                                  : RS_PORT_CMD_REQUEST_SWITCH_CHANNEL,
                              cmd);

            } else if (msec_diff(now, layer->ports[i]->cmd_switch_state.at) >
                       0) {
                /* finish switch or request */

                if (layer->ports[i]->cmd_switch_state.state ==
                        RS_PORT_CMD_SWITCH_FOLLOWING ||
                    layer->ports[i]->cmd_switch_state.state ==
                        RS_PORT_CMD_SWITCH_OWNING) {

                    layer->ports[i]->bound_channel =
                        layer->ports[i]->cmd_switch_state.new_channel;
                }

                layer->ports[i]->cmd_switch_state.state =
                    RS_PORT_CMD_SWITCH_NONE;
            }
        }

        /* Heartbeats */
        for (int i = 0; i < layer->n_ports; i++) {
            long msec = msec_diff(now, layer->ports[i]->tx_last_ts);

            if (msec >= RS_PORT_CMD_HEARTBEAT_MSEC) {
                uint8_t cmd[RS_PORT_LAYER_COMMAND_LENGTH] = {0, 0, 0, 0,
                                                             0, 0, 0, 0};
                _send_command(layer, layer->ports[i], RS_PORT_CMD_HEARTBEAT,
                              cmd);
            }
        }
    }
}

int rs_port_layer_switch_channel(struct rs_port_layer *layer, rs_port_id_t port,
                                 rs_channel_t new_channel) {
    struct rs_port *p = NULL;
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i]->id == port) {
            p = layer->ports[i];
            break;
        }
    }
    if (!p) {
        syslog(LOG_ERR, "Unknown port");
        return -1;
    }

    p->cmd_switch_state.n_broadcasts = 0;
    clock_gettime(CLOCK_REALTIME, &p->cmd_switch_state.begin);
    p->cmd_switch_state.new_channel = new_channel;
    p->cmd_switch_state.at = p->cmd_switch_state.begin;
    timespec_plus_ms(&p->cmd_switch_state.at,
                     RS_PORT_CMD_SWITCH_N_BROADCAST *
                         RS_PORT_CMD_SWITCH_DT_BROADCAST_MSEC);

    if (p->owner) {
        p->cmd_switch_state.state = RS_PORT_CMD_SWITCH_OWNING;
    } else {
        p->cmd_switch_state.state = RS_PORT_CMD_SWITCH_REQUESTING;
    }

    return 0;
}

void rs_port_layer_stats_printf(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        printf("-------- %04X --------\n", layer->ports[i]->id);
        rs_stats_printf(&layer->ports[i]->stats);
    }
}
