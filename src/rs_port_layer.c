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
        rs_port_layer_create_port(layer, config_setting_get_elem(c, i));
    }

    /* connect heartbeat routes */
    for (int i = 0; i < layer->n_ports; i++) {
        if (layer->ports[i]->route_cmd.config == RS_PORT_CMD_ROUTE) {
            rs_port_id_t via = layer->ports[i]->route_cmd.route_via_id;
            layer->ports[i]->route_cmd.route_via = NULL;

            for (int j = 0; j < layer->n_ports; j++) {
                if (layer->ports[j]->id == via) {
                    int n = 0;
                    for (void *p = layer->ports[j]->route_cmd.routing_via_this;
                         p; p++)
                        n++;
                    layer->ports[j]->route_cmd.routing_via_this = realloc(
                        layer->ports[j]->route_cmd.routing_via_this, n + 1);

                    layer->ports[j]->route_cmd.routing_via_this[n] =
                        layer->ports[i];
                    layer->ports[j]->route_cmd.routing_via_this[n + 1] = NULL;

                    layer->ports[i]->route_cmd.route_via = layer->ports[j];
                }
            }

            if (!layer->ports[i]->route_cmd.route_via) {
                syslog(LOG_ERR, "Could not create heartbeat route %d -> %d",
                       layer->ports[i]->id, via);
                layer->ports[i]->route_cmd.config = RS_PORT_CMD_REGULAR;
            }
        }
    }
}

void rs_port_layer_create_port(struct rs_port_layer *layer,
                               config_setting_t *config) {

    int port;
    config_setting_lookup_int(config, "id", &port);

    int bound_channel;
    config_setting_lookup_int(config, "bound_channel", &bound_channel);

    int owner = layer->server->other_id;
    config_setting_lookup_int(config, "owner", &owner);
    owner = (owner == layer->server->own_id);

    int max_packet_size = 1024;
    config_setting_lookup_int(config, "max_packet_size", &max_packet_size);

    int fec_k = 8;
    config_setting_lookup_int(config, "fec_k", &fec_k);

    int fec_m = 12;
    config_setting_lookup_int(config, "fec_m", &fec_m);

    int route_cmd = -100;
    config_setting_lookup_int(config, "route_cmd", &route_cmd);

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
    new_port->bound_channel = bound_channel;
    new_port->tx_last_seq = 0;
    new_port->owner = owner;
    memset(&new_port->frag_buffer, 0, sizeof(new_port->frag_buffer));
    rs_stats_init(&new_port->stats);
    rs_stat_init(&new_port->tx_stats_fec_factor, RS_STAT_AGG_AVG, "TX FEC", "",
                 1.);
    rs_stat_init(&new_port->rx_stats_fec_factor, RS_STAT_AGG_AVG, "RX FEC", "",
                 1.);

    new_port->tx_fec = NULL;
    new_port->rx_fec = NULL;
    rs_port_setup_tx_fec(new_port, max_packet_size, fec_k, fec_m);
    rs_port_setup_rx_fec(new_port, fec_k, fec_m);

    clock_gettime(CLOCK_REALTIME, &new_port->tx_last_ts);

    layer->n_ports++;
    layer->ports = realloc(layer->ports, layer->n_ports * sizeof(void *));
    layer->ports[layer->n_ports - 1] = new_port;

    new_port->route_cmd.routing_via_this = calloc(1, sizeof(void *));
    *(new_port->route_cmd.routing_via_this) = NULL;
    if (route_cmd == -1) {
        new_port->route_cmd.config = RS_PORT_CMD_DISABLE;
    } else if (route_cmd >= 0) {
        new_port->route_cmd.config = RS_PORT_CMD_ROUTE;
        new_port->route_cmd.route_via_id = route_cmd;
    } else {
        new_port->route_cmd.config = RS_PORT_CMD_REGULAR;
    }
}

void rs_port_setup_tx_fec(struct rs_port *port, int max_packet_size, int k,
                          int m) {
    port->tx_max_packet_size = max_packet_size;
    if (!port->tx_fec || port->tx_fec_k != k || port->tx_fec_m != m) {
        port->tx_fec_m = m;
        port->tx_fec_k = k;
        if (port->tx_fec)
            fec_free(port->tx_fec);
        port->tx_fec = fec_new(k, m);
    }
}
void rs_port_setup_rx_fec(struct rs_port *port, int k, int m) {
    if (!port->rx_fec || port->rx_fec_k != k || port->rx_fec_m != m) {
        port->rx_fec_m = m;
        port->rx_fec_k = k;
        if (port->rx_fec)
            fec_free(port->rx_fec);
        port->rx_fec = fec_new(k, m);
    }
}

void rs_port_layer_destroy(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        fec_free(layer->ports[i]->tx_fec);
        fec_free(layer->ports[i]->rx_fec);
        for (int j = 0; j < layer->ports[i]->frag_buffer.n_frag_received; j++) {
            rs_packet_destroy(
                &layer->ports[i]->frag_buffer.fragments[j]->super);
            free(layer->ports[i]->frag_buffer.fragments[j]);
        }
        free(layer->ports[i]->frag_buffer.fragments);
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

    int n_fragments = rs_port_layer_packet_split(packet, port, &fragments);
    rs_stat_register(&port->tx_stats_fec_factor,
                     (double)fragments[0]->n_frag_encoded /
                         (double)fragments[0]->n_frag_decoded);

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

    int res;
    packet->port = port->id;
    packet->seq = port->tx_last_seq + 1;

    if ((res = _transmit_fragmented(layer, packet, port, ch)) >= 0) {
        port->tx_last_seq++;
        clock_gettime(CLOCK_REALTIME, &port->tx_last_ts);

        /* Register stats */
        rs_stats_register_tx(&port->stats, packet->payload_len);
    }
    return res;
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

    int res = _transmit(layer, &packed, p);

    rs_packet_destroy(&packed.super);
    return res;
}

#define RS_PORT_LAYER_INCOMPLETE 2

static int _receive_fragmented(struct rs_port_layer *layer,
                               struct rs_port_layer_packet *fragment,
                               struct rs_port *port,
                               struct rs_port_layer_packet **packet_ret) {
    if (fragment->n_frag_encoded == 1) {
        *packet_ret = fragment;
        return 0;
    }

    if (fragment->seq != port->frag_buffer.seq) {
        port->frag_buffer.seq = fragment->seq;
        port->frag_buffer.n_frag_received = 0;
        for (int i = 0; i < port->frag_buffer.n_frag; i++) {
            if (port->frag_buffer.fragments[i]) {
                rs_packet_destroy(&port->frag_buffer.fragments[i]->super);
                free(port->frag_buffer.fragments[i]);
            }
        }
        port->frag_buffer.n_frag = fragment->n_frag_encoded;
        port->frag_buffer.fragments =
            realloc(port->frag_buffer.fragments,
                    fragment->n_frag_encoded * sizeof(void *));
        for (int i = 0; i < port->frag_buffer.n_frag; i++) {
            port->frag_buffer.fragments[i] = NULL;
        }
    }

    int new_fragment = 1;
    for (int i = 0; i < port->frag_buffer.n_frag_received; i++) {
        if (port->frag_buffer.fragments[i]->frag == fragment->frag) {
            new_fragment = 0;
            break;
        }
    }

    if (new_fragment) {
        port->frag_buffer.fragments[port->frag_buffer.n_frag_received] =
            fragment;
        port->frag_buffer.n_frag_received++;
    }

    if (port->frag_buffer.n_frag_received == fragment->n_frag_decoded) {
        *packet_ret = calloc(1, sizeof(struct rs_port_layer_packet));

        rs_stat_register(
            &port->rx_stats_fec_factor,
            (double)port->frag_buffer.fragments[0]->n_frag_encoded /
                (double)port->frag_buffer.fragments[0]->n_frag_decoded);
        int res = rs_port_layer_packet_join(*packet_ret, port,
                                            port->frag_buffer.fragments,
                                            port->frag_buffer.n_frag_received);
        if (res) {
            free(*packet_ret);
            *packet_ret = NULL;
        }
        return res;
    }

    return RS_PORT_LAYER_INCOMPLETE;
}

static int _receive(struct rs_port_layer *layer, struct rs_packet **packet_ret,
                    rs_port_id_t *port_ret, rs_channel_t channel) {
    struct rs_channel_layer *ch =
        rs_server_channel_layer_for_channel(layer->server, channel);

    if (!ch) {
        syslog(LOG_ERR, "Could not find layer for channel %d", channel);
        return -1;
    }

    struct rs_packet *packet = NULL;
    struct rs_port_layer_packet *unpacked =
        calloc(1, sizeof(struct rs_port_layer_packet));

retry:
    switch (rs_channel_layer_receive(ch, &packet, &channel)) {
    case 0:
        if (rs_port_layer_packet_unpack(unpacked, packet)) {
            /* packed that could not be unpacked */
            syslog(LOG_ERR, "Could not unpack at port layer");
            free(packet);
            free(unpacked);
            return -1;
        }
        rs_packet_destroy(packet);
        free(packet);
        packet = NULL;

        struct rs_port *port = NULL;
        for (int i = 0; i < layer->n_ports; i++) {
            if (layer->ports[i]->id == unpacked->port) {
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

        struct rs_port_layer_packet *result;
        int res = _receive_fragmented(layer, unpacked, port, &result);
        /* unpacked is possibly invalid by now, ownership n any case transferred
         */
        unpacked = calloc(1, sizeof(struct rs_port_layer_packet));

        if (!res) {
            if (result->seq == port->rx_last_seq) {
                syslog(LOG_DEBUG, "Duplicate packet");
                rs_packet_destroy(&result->super);
                free(result);

                goto retry;
            }

            rs_stats_register_rx(&port->stats, result->payload_len,
                                 result->seq - port->rx_last_seq - 1,
                                 &result->stats);
            port->rx_last_seq = result->seq;

            if (result->command) {
                /* Received a command packet -> dispatch only after registering
                 * stats */
                rs_port_layer_main(layer, result);
                rs_packet_destroy(&result->super);
                free(result);
                goto retry;
            }

            *port_ret = result->port;

            *packet_ret = calloc(1, sizeof(struct rs_packet));
            rs_packet_init(*packet_ret, result->super.payload_ownership,
                           result->super.payload_packet,
                           result->super.payload_data,
                           result->super.payload_data_len);
            result->super.payload_ownership = NULL;
            rs_packet_destroy(&result->super);

            if (result != unpacked) {
                free(result);
            }

            free(unpacked);
            return 0;
        } else {
            goto retry;
        }

        break;
    case RS_CHANNEL_LAYER_EOF:
        /* No more packets */
        free(unpacked);
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
        free(unpacked);
        return -1;
    }
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

    /* Possibly route the packet */
    assert(sizeof(rs_port_id_t) == 1);
    if (port->route_cmd.config == RS_PORT_CMD_ROUTE) {
        packet.command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 2] =
            RS_PORT_CMD_MARKER_ROUTED;
        packet.command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 1] = port->id;

        /* a bit hacky, but ensure tx_last_ts is set on original port */
        clock_gettime(CLOCK_REALTIME, &port->tx_last_ts);

        port = port->route_cmd.route_via;
    }

    _transmit(layer, &packet, port);

    rs_packet_destroy(&packet.super);
}

void rs_port_layer_main(struct rs_port_layer *layer,
                        struct rs_port_layer_packet *received) {

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (received) {
        /* Handle routed commands */
        assert(sizeof(rs_port_id_t) == 1);
        if (received->command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 2] ==
            RS_PORT_CMD_MARKER_ROUTED) {
            received->port =
                received->command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 1];
            received->command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 2] = 0;
            received->command_payload[RS_PORT_LAYER_COMMAND_LENGTH - 1] = 0;
        }

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
        /* ensure all channels (needed for TX) and no more are opened */
        for (int i = 0; i < layer->server->n_channel_layers; i++) {
            rs_channel_layer_close_all_channels(
                layer->server->channel_layers[i]);
            for (int j = 0; j < layer->n_ports; j++) {
                if (rs_channel_layer_owns_channel(
                        layer->server->channel_layers[i],
                        layer->ports[j]->bound_channel) &&
                    layer->ports[j]->route_cmd.config == RS_PORT_CMD_REGULAR) {

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

                assert(sizeof(rs_channel_t) == 2);
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

int rs_port_layer_update_port(struct rs_port_layer *layer, rs_port_id_t port,
                              int max_packet_size, int fec_k, int fec_m) {
    /* TODO!
     * These are two independent parameters to be configured - should be
     * improved.
     * Also FEC is only active for packets below max_packet_size
     *  * small data packets should have FEC (be sent twice e.g.) nonetheless
     *  * heartbeats should not...
     */
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

    if (!p->owner)
        return -1;

    rs_port_setup_tx_fec(p, max_packet_size, fec_k, fec_m);

    return 0;
}

void rs_port_layer_stats_printf(struct rs_port_layer *layer) {
    for (int i = 0; i < layer->n_ports; i++) {
        printf("-------- %04X --------\n", layer->ports[i]->id);
        rs_stats_printf(&layer->ports[i]->stats);
    }
}
