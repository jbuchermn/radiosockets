#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>

#include "rs_app_layer.h"
#include "rs_packet.h"
#include "rs_port_layer.h"
#include "rs_server_state.h"

void rs_app_layer_init(struct rs_app_layer *layer,
                       struct rs_server_state *server) {
    layer->server = server;
    layer->connections = NULL;
    layer->n_connections = 0;

    config_setting_t *c = config_lookup(&server->config, "apps");
    int n_conn_conf = c ? config_setting_length(c) : 0;
    for (int i = 0; i < n_conn_conf; i++) {
        char p[100];

        int port;
        sprintf(p, "apps.[%d].port", i);
        config_lookup_int(&server->config, p, &port);

        int udp_port;
        sprintf(p, "apps.[%d].udp", i);
        config_lookup_int(&server->config, p, &udp_port);

        int frame_size = 512;
        sprintf(p, "apps.[%d].frame_size", i);
        config_lookup_int(&server->config, p, &frame_size);

        rs_app_layer_open_connection(layer, port, udp_port, frame_size);
    }
}

void rs_app_layer_open_connection(struct rs_app_layer *layer, rs_port_id_t port,
                                  int udp_port, int frame_size) {

    /* open udp socket */
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        syslog(LOG_ERR, "app layer: Could not create socket");
        return;
    }

    struct sockaddr_in addr_server;

    memset(&addr_server, 0, sizeof(struct sockaddr_in));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(udp_port);
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr_server,
             sizeof(struct sockaddr_in))) {
        syslog(LOG_ERR, "app layer: Could not bind socket");
        return;
    }

    int flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG,
           "app layer: Server listening on socket udp://localhost:%d",
           udp_port);

    layer->n_connections++;
    layer->connections =
        realloc(layer->connections, layer->n_connections * sizeof(void *));

    struct rs_app_connection *new_conn;
    layer->connections[layer->n_connections - 1] =
        (new_conn = calloc(1, sizeof(struct rs_app_connection)));

    rs_stat_init(&new_conn->stat, RS_STAT_AGG_SUM, "UDP", "bps",
                 1000. / RS_STAT_DT_MSEC);
    new_conn->port = port;
    new_conn->frame_size = frame_size;
    new_conn->buffer_size = RS_APP_CONNECTION_BUFFER * new_conn->frame_size;
    new_conn->buffer = calloc(new_conn->buffer_size, sizeof(uint8_t));
    new_conn->buffer_begin_at = 0;
    new_conn->buffer_at = 0;

    new_conn->socket = sock;
    new_conn->addr_server = addr_server;
}

void rs_app_layer_main(struct rs_app_layer *layer, struct rs_packet *received,
                       rs_port_id_t received_port) {

    if (received) {
        for (int i = 0; i < layer->n_connections; i++) {
            if (layer->connections[i]->port != received_port)
                continue;

            if (!layer->connections[i]->addr_client_len)
                continue;

            if (sendto(layer->connections[i]->socket, received->payload_data,
                       received->payload_data_len, 0,
                       (struct sockaddr *)&layer->connections[i]->addr_client,
                       layer->connections[i]->addr_client_len)) {
                syslog(LOG_ERR, "Send failed");
            }
        }
    } else {
        for (int i = 0; i < layer->n_connections; i++) {
            socklen_t slen;

            struct rs_app_connection *conn = layer->connections[i];

            int recv_len =
                recvfrom(conn->socket, conn->buffer + conn->buffer_at,
                         conn->buffer_size - conn->buffer_at, 0,
                         (struct sockaddr *)&conn->addr_client, &slen);

            if (recv_len < 0) {
                continue;
            }
            /* We have a client */
            conn->addr_client_len = slen;

            conn->buffer_at += recv_len;
            rs_stat_register(&conn->stat, 8 * recv_len);

            while (conn->buffer_at - conn->buffer_begin_at >=
                   conn->frame_size) {

                struct rs_packet packet;
                rs_packet_init(&packet, NULL, NULL,
                               conn->buffer + conn->buffer_begin_at,
                               conn->frame_size);
                rs_port_layer_transmit(layer->server->port_layer, &packet,
                                       conn->port);
                rs_packet_destroy(&packet);

                conn->buffer_begin_at += conn->frame_size;
            }

            if (conn->buffer_begin_at) {
                memcpy(conn->buffer, conn->buffer + conn->buffer_begin_at,
                       sizeof(uint8_t) *
                           (conn->buffer_at - conn->buffer_begin_at));
                conn->buffer_at -= conn->buffer_begin_at;
                conn->buffer_begin_at = 0;
            }
        }
    }
}

void rs_app_connection_destroy(struct rs_app_connection *connection) {
    close(connection->socket);
}

void rs_app_layer_destroy(struct rs_app_layer *layer) {
    for (int i = 0; i < layer->n_connections; i++) {
        rs_app_connection_destroy(layer->connections[i]);
        free(layer->connections[i]->buffer);
        free(layer->connections[i]);
    }

    free(layer->connections);
}
