#include <fcntl.h>
#include <signal.h>
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

    /* Very important as the call to write on a closed socket results in
     * SIGPIPE, without handling the signal the call to write blocks */
    signal(SIGPIPE, SIG_IGN);

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

    const char* _frame_sep;
    if(config_setting_lookup_string(conf, "frame_sep", &_frame_sep) == CONFIG_TRUE){
        int len = strlen(_frame_sep);
        if(!len || len%2){
            syslog(LOG_ERR, "Invalid frame_sep");
            return -1;
        }

        frame_sep_size = len/2;
        frame_sep = calloc(frame_sep_size, sizeof(uint8_t));
        for(int i=0; i<frame_sep_size; i++){
            char c[3];
            c[0] = _frame_sep[2*i];
            c[1] = _frame_sep[2*i + 1];
            c[2] = 0;

            frame_sep[i] = strtol((const char*)&c, NULL, 16);
        }
    }

    if (port < 0) {
        syslog(LOG_ERR, "Need to specify port");
        return -1;
    }
    if (tcp_port < 0) {
        syslog(LOG_ERR, "Need to specify tcp_port");
        return -1;
    }

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

    rs_frame_buffer_init(&new_conn->buffer,
                         frame_size_fixed > 0 ? frame_size_fixed
                                              : RS_APP_BUFFER_DEFAULT_SIZE,
                         N_FRAMES);

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

            int res = write(layer->connections[i]->client_socket,
                            received->payload_data, received->payload_data_len);
            if (res < 0) {
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

            /* collect all frames possibly skipping some */
            for (;;) {
                int recv_len =
                    recv(conn->client_socket,
                         conn->buffer.buffer + conn->buffer.buffer_at,
                         conn->buffer.buffer_size - conn->buffer.buffer_at, 0);

                if (recv_len <= 0) {
                    break;
                }

                rs_stat_register(&conn->stat_in, 8 * recv_len);

                if (conn->frame_size_fixed > 0) {
                    rs_frame_buffer_process_fixed_size(&conn->buffer, recv_len,
                                                       conn->frame_size_fixed);
                } else {
                    rs_frame_buffer_process(&conn->buffer, recv_len,
                                            conn->frame_sep,
                                            conn->frame_sep_size);
                }

                if (conn->buffer.n_frames >= N_FRAMES - 1) {
                    rs_frame_buffer_flush(&conn->buffer, 1);
                }
            }

            /* Did we skip frames */
            for (; conn->buffer.ext_at_frame < 0; conn->buffer.ext_at_frame++)
                rs_stat_register(&conn->stat_skipped, 1);

            /* send one frame */
            if (conn->buffer.n_frames > conn->buffer.ext_at_frame) {
                rs_stat_register(&conn->stat_skipped, 0.0);

                struct rs_packet packet;

                rs_packet_init(
                    &packet, NULL, NULL,
                    conn->buffer.buffer +
                        conn->buffer.frame_start[conn->buffer.ext_at_frame],
                    conn->buffer.frame_start[conn->buffer.ext_at_frame + 1] -
                        conn->buffer.frame_start[conn->buffer.ext_at_frame]);
                rs_port_layer_transmit(layer->server->port_layer, &packet,
                                       conn->port);
                rs_packet_destroy(&packet);

                conn->buffer.ext_at_frame++;
            }
        }
    }
}

void rs_app_connection_destroy(struct rs_app_connection *connection) {
    if (connection->client_socket > 0)
        close(connection->client_socket);
    close(connection->socket);
    rs_frame_buffer_destroy(&connection->buffer);
    free(connection->frame_sep);
}

void rs_app_layer_destroy(struct rs_app_layer *layer) {
    for (int i = 0; i < layer->n_connections; i++) {
        rs_app_connection_destroy(layer->connections[i]);
        free(layer->connections[i]);
    }

    free(layer->connections);
}

void rs_frame_buffer_init(struct rs_frame_buffer *buffer,
                          int expected_frame_size, int n_frames_max) {
    buffer->n_frames_max = n_frames_max;
    buffer->frame_start = calloc(buffer->n_frames_max + 1, sizeof(int));
    buffer->n_frames = 0;
    buffer->frame_start[0] = 0;

    buffer->buffer_size = buffer->n_frames_max * expected_frame_size;
    buffer->buffer = calloc(buffer->buffer_size, sizeof(uint8_t));
    buffer->buffer_at = 0;
}

void rs_frame_buffer_destroy(struct rs_frame_buffer *buffer) {
    free(buffer->buffer);
    free(buffer->frame_start);
}

void rs_frame_buffer_process_fixed_size(struct rs_frame_buffer *buffer,
                                        int new_len, int frame_size_fixed) {

    if (buffer->buffer_size < frame_size_fixed * buffer->n_frames_max) {
        buffer->buffer_size = frame_size_fixed * buffer->n_frames_max;
        buffer->buffer =
            realloc(buffer->buffer, buffer->buffer_size * sizeof(uint8_t));
    }

    buffer->buffer_at += new_len;
    while (buffer->n_frames < buffer->n_frames_max &&
           buffer->buffer_at - buffer->frame_start[buffer->n_frames] >=
               frame_size_fixed) {

        if (buffer->n_frames == buffer->n_frames_max) {
            memcpy(buffer->frame_start, buffer->frame_start + 1,
                   buffer->n_frames_max * sizeof(int));
            buffer->n_frames--;
            buffer->ext_at_frame--;
        }

        buffer->n_frames++;
        buffer->frame_start[buffer->n_frames] =
            buffer->frame_start[buffer->n_frames - 1] + frame_size_fixed;
    }
}

void rs_frame_buffer_process(struct rs_frame_buffer *buffer, int new_len,
                             uint8_t *sep, int sep_len) {
    int start_looking = buffer->buffer_at - sep_len + 1;
    if (start_looking < 0)
        start_looking = 0;

    buffer->buffer_at += new_len;
    int stop_looking = buffer->buffer_at - sep_len;

    for (int i = start_looking; i < stop_looking; i++) {
        int is_sep = 1;
        for (int j = 0; j < sep_len; j++) {
            if (buffer->buffer[i + j] != sep[j]) {
                is_sep = 0;
                break;
            }
        }

        if (is_sep) {
            if (buffer->n_frames == buffer->n_frames_max) {
                memcpy(buffer->frame_start, buffer->frame_start + 1,
                       buffer->n_frames_max * sizeof(int));
                buffer->n_frames--;
                buffer->ext_at_frame--;
            }

            buffer->n_frames++;
            buffer->frame_start[buffer->n_frames] = i;
        }
    }

    if (buffer->buffer_at == buffer->buffer_size &&
        buffer->n_frames < buffer->n_frames_max) {

        /* Appears our buffer is too small */
        buffer->buffer_size *= 2;
        if (buffer->buffer_size > RS_FRAME_BUFFER_MAX_SIZE) {
            buffer->buffer_size = RS_FRAME_BUFFER_MAX_SIZE;
            syslog(LOG_ERR, "Reached maximum frame buffer size - probably "
                            "there is an issue with frame separators");
        }

        buffer->buffer =
            realloc(buffer->buffer, buffer->buffer_size * sizeof(uint8_t));
    }
}

void rs_frame_buffer_flush(struct rs_frame_buffer *buffer, int keep_n_frames) {
    if (keep_n_frames > buffer->n_frames)
        keep_n_frames = buffer->n_frames;

    int copy_n = buffer->buffer_at -
                 buffer->frame_start[buffer->n_frames - keep_n_frames];
    int delta_n = buffer->frame_start[buffer->n_frames - keep_n_frames];

    memcpy(buffer->buffer, buffer->buffer + delta_n, copy_n * sizeof(uint8_t));
    buffer->buffer_at = copy_n;

    for (int i = 0; i < keep_n_frames + 1; i++) {
        buffer->frame_start[i] =
            buffer->frame_start[i + (buffer->n_frames - keep_n_frames)] -
            delta_n;
    }

    buffer->ext_at_frame -= buffer->n_frames - keep_n_frames;
    buffer->n_frames = keep_n_frames;
}
