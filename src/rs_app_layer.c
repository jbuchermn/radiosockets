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
#include "rs_util.h"

/* TODO make this dependent on frame size, or at least configurable per app */
#define N_FRAMES 10

void rs_app_layer_init(struct rs_app_layer *layer,
                       struct rs_server_state *server) {
    layer->server = server;
    layer->connections = NULL;
    layer->n_connections = 0;

    config_setting_t *c = config_lookup(&server->config, "apps");
    int n_conn_conf = c ? config_setting_length(c) : 0;
    for (int i = 0; i < n_conn_conf; i++) {
        char p[100];
        sprintf(p, "apps.[%d]", i);
        rs_app_layer_open_connection(layer, config_lookup(&server->config, p));
    }
}

int rs_app_layer_open_connection(struct rs_app_layer *layer,
                                 config_setting_t *conf) {
    int port = -1;
    int tcp_port = -1;
    int frame_size_fixed = -1;

    uint8_t *frame_sep = NULL;
    int frame_sep_size = -1;

    config_setting_lookup_int(conf, "port", &port);
    config_setting_lookup_int(conf, "tcp", &tcp_port);
    config_setting_lookup_int(conf, "frame_size_fixed", &frame_size_fixed);

    if (port < 0) {
        syslog(LOG_ERR, "Need to specify port");
        return -1;
    }
    if (tcp_port < 0) {
        syslog(LOG_ERR, "Need to specify tcp_port");
        return -1;
    }

    /* TODO: Read frame_sep */

    if (frame_sep_size < 0 && frame_size_fixed < 0) {
        syslog(LOG_NOTICE, "app layer: frame size not specified, defaulting to "
                           "fixed size 1kb");
        frame_size_fixed = 1024;
    }

    /* open tcp socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        syslog(LOG_ERR, "app layer: Could not create socket");
        return -1;
    }

    struct sockaddr_in addr_server;

    memset(&addr_server, 0, sizeof(struct sockaddr_in));
    addr_server.sin_family = AF_INET;
    addr_server.sin_port = htons(tcp_port);
    addr_server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
        syslog(LOG_ERR, "app layer: Could not set REUSEADDR | REUSEPORT");
    }

    int err;
    if ((err = bind(sock, (struct sockaddr *)&addr_server,
                    sizeof(struct sockaddr_in)))) {
        syslog(LOG_ERR, "app layer: Could not bind socket: %d", err);
        return -1;
    }
    listen(sock, 5);

    /* put it in non-blocking mode */
    int flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    syslog(LOG_DEBUG,
           "app layer: Server listening on socket tcp://localhost:%d",
           tcp_port);

    /* initialize struct */
    layer->n_connections++;
    layer->connections =
        realloc(layer->connections, layer->n_connections * sizeof(void *));

    struct rs_app_connection *new_conn;
    layer->connections[layer->n_connections - 1] =
        (new_conn = calloc(1, sizeof(struct rs_app_connection)));

    rs_stat_init(&new_conn->stat_in, RS_STAT_AGG_SUM, "APP", "bps",
                 1000. / RS_STAT_DT_MSEC);
    rs_stat_init(&new_conn->stat_skipped, RS_STAT_AGG_AVG, "APP", "", 1.);
    new_conn->port = port;
    new_conn->frame_size_fixed = frame_size_fixed;
    new_conn->frame_sep = frame_sep;
    new_conn->frame_sep_size = frame_sep_size;

    new_conn->buffer_size =
        frame_size_fixed > 0 ? N_FRAMES * frame_size_fixed : RS_APP_BUFFER_SIZE;
    new_conn->buffer = calloc(new_conn->buffer_size, sizeof(uint8_t));
    new_conn->buffer_at = 0;
    new_conn->buffer_start_frame = 0;

    new_conn->socket = sock;
    new_conn->addr_server = addr_server;
    new_conn->client_socket = -1;

    return 0;
}

void rs_app_layer_main(struct rs_app_layer *layer, struct rs_packet *received,
                       rs_port_id_t received_port) {

    /* Accept connections */
    for (int i = 0; i < layer->n_connections; i++) {
        int fd = accept(layer->connections[i]->socket, NULL, NULL);
        if (fd >= 0) {
            if (layer->connections[i]->client_socket >= 0) {
                syslog(LOG_NOTICE, "app layer: Closed connection on port %d\n",
                       layer->connections[i]->port);
                close(layer->connections[i]->client_socket);
            }

            syslog(LOG_NOTICE, "app layer: Accepted connection on port %d\n",
                   layer->connections[i]->port);
            /* put it in non-blocking mode */
            int flags = fcntl(fd, F_GETFL);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);

            layer->connections[i]->client_socket = fd;
        }
    }

    /* Handle received packets */
    if (received) {
        for (int i = 0; i < layer->n_connections; i++) {
            if (layer->connections[i]->port != received_port)
                continue;

            if (layer->connections[i]->client_socket < 0)
                continue;

            if (write(layer->connections[i]->client_socket,
                      received->payload_data, received->payload_data_len) < 0) {
                close(layer->connections[i]->client_socket);
                layer->connections[i]->client_socket = -1;
                syslog(LOG_NOTICE, "app layer: Closed connection on port %d\n",
                       layer->connections[i]->port);
            }
        }
    } else {
        /* Send packets */
        for (int i = 0; i < layer->n_connections; i++) {
            struct rs_app_connection *conn = layer->connections[i];
            if (conn->client_socket < 0)
                continue;

            /* TODO sep-mode */

            /* collect all frames */
            for (;;) {
                int recv_len =
                    recv(conn->client_socket, conn->buffer + conn->buffer_at,
                         conn->buffer_size - conn->buffer_at, 0);

                if (recv_len <= 0) {
                    break;
                }

                conn->buffer_at += recv_len;
                rs_stat_register(&conn->stat_in, 8 * recv_len);

                if ((conn->buffer_at - conn->buffer_start_frame +
                     conn->buffer_size) %
                        conn->buffer_size >=
                    (N_FRAMES - 1) * conn->frame_size_fixed) {

                    for (int j = 0; j < N_FRAMES - 1; j++)
                        rs_stat_register(&conn->stat_skipped, 1);
                    conn->buffer_start_frame =
                        (conn->buffer_start_frame +
                         N_FRAMES * conn->frame_size_fixed) %
                        conn->buffer_size;
                }

                if (conn->buffer_at == conn->buffer_size) {
                    conn->buffer_at = 0;
                }
            }

            /* send one frame */
            if ((conn->buffer_at - conn->buffer_start_frame +
                 conn->buffer_size) %
                    conn->buffer_size >=
                conn->frame_size_fixed) {

                rs_stat_register(&conn->stat_skipped, 0.0);

                struct rs_packet packet;
                if (conn->buffer_size - conn->buffer_start_frame >=
                    conn->frame_size_fixed) {

                    rs_packet_init(&packet, NULL, NULL,
                                   conn->buffer + conn->buffer_start_frame,
                                   conn->frame_size_fixed);
                } else {
                    uint8_t *tmp = calloc(conn->frame_size_fixed, 1);
                    memcpy(tmp, conn->buffer + conn->buffer_start_frame,
                           conn->buffer_size - conn->buffer_start_frame);
                    memcpy(tmp + (conn->buffer_size - conn->buffer_start_frame),
                           conn->buffer,
                           conn->frame_size_fixed -
                               (conn->buffer_size - conn->buffer_start_frame));
                    rs_packet_init(&packet, tmp, NULL,
                                   conn->buffer + conn->buffer_start_frame,
                                   conn->frame_size_fixed);
                }
                rs_port_layer_transmit(layer->server->port_layer, &packet,
                                       conn->port);
                rs_packet_destroy(&packet);

                conn->buffer_start_frame =
                    (conn->buffer_start_frame + conn->frame_size_fixed) %
                    conn->buffer_size;
            }
        }
    }
}

void rs_app_connection_destroy(struct rs_app_connection *connection) {
    if (connection->client_socket > 0)
        close(connection->client_socket);
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
