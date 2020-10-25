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
}

void rs_app_layer_open_connection(struct rs_app_layer *layer, rs_port_id_t port,
                                  int udp_port) {

    layer->n_connections++;
    layer->connections =
        realloc(layer->connections, layer->n_connections * sizeof(void *));

    struct rs_app_connection *new_conn;
    layer->connections[layer->n_connections - 1] =
        (new_conn = calloc(1, sizeof(struct rs_app_connection)));

    new_conn->port = port;

    /* open udp socket */
    new_conn->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (new_conn->socket == -1) {
        syslog(LOG_ERR, "Could not create socket");
        return;
    }
    new_conn->addr_client_len = 0;

    memset(&new_conn->addr_server, 0, sizeof(struct sockaddr_in));
    new_conn->addr_server.sin_family = AF_INET;
    new_conn->addr_server.sin_port = htons(udp_port);
    new_conn->addr_server.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(new_conn->socket, (struct sockaddr *)&new_conn->addr_server,
             sizeof(struct sockaddr_in))) {
        syslog(LOG_ERR, "Could not bind socket");
        return;
    }

    int flags = fcntl(new_conn->socket, F_GETFL);
    fcntl(new_conn->socket, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG,
           "app layer: Server listening on socket udp://localhost:%d",
           udp_port);
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
        /* TODO: Configure frame size in app layer */
        uint8_t buf[1024];
        int buf_len = 1024;

        for (int i = 0; i < layer->n_connections; i++) {
            socklen_t slen;
            int recv_len = recvfrom(
                layer->connections[i]->socket, buf, buf_len, 0,
                (struct sockaddr *)&layer->connections[i]->addr_client, &slen);

            if (recv_len < 0) {
                continue;
            }

            layer->connections[i]->addr_client_len = slen;

            struct rs_packet packet;
            rs_packet_init(&packet, NULL, NULL, buf, recv_len);
            rs_port_layer_transmit(layer->server->port_layer, &packet,
                                   layer->connections[i]->port);
            rs_packet_destroy(&packet);
        }
    }
}

void rs_app_connection_destroy(struct rs_app_connection *connection) {
    close(connection->socket);
}

void rs_app_layer_destroy(struct rs_app_layer *layer) {
    for (int i = 0; i < layer->n_connections; i++) {
        rs_app_connection_destroy(layer->connections[i]);
        free(layer->connections[i]);
    }

    free(layer->connections);
}
