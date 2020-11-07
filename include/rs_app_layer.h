#ifndef RS_APP_LAYER_H
#define RS_APP_LAYER_H

#include <arpa/inet.h>
#include <libconfig.h>

#include "rs_port_layer.h"
#include "rs_stat.h"

struct rs_server_state;
struct rs_app_connection;

struct rs_app_layer {
    struct rs_server_state *server;

    struct rs_app_connection **connections;
    int n_connections;
};

void rs_app_layer_init(struct rs_app_layer *layer,
                       struct rs_server_state *server);

int rs_app_layer_open_connection(struct rs_app_layer *layer,
                                 config_setting_t *conf);
void rs_app_layer_destroy(struct rs_app_layer *layer);
void rs_app_connection_destroy(struct rs_app_connection *connection);

void rs_app_layer_main(struct rs_app_layer *layer, struct rs_packet *received,
                       rs_port_id_t received_port);

#define RS_FRAME_BUFFER_MAX_SIZE 10000000

struct rs_frame_buffer {
    uint8_t *buffer;
    int buffer_at;
    int buffer_size;

    int *frame_start;
    int n_frames;
    int n_frames_max;

    /* externally selected frame - will be preserved during flush */
    int ext_at_frame;
};

void rs_frame_buffer_init(struct rs_frame_buffer *buffer,
                          int expected_frame_size, int n_frames_max);
void rs_frame_buffer_destroy(struct rs_frame_buffer *buffer);

/* Update frame_start and n_frames based on new_len bytes of new data, which has
 * been placed into buffer + buffer_at */
void rs_frame_buffer_process_fixed_size(struct rs_frame_buffer *buffer,
                                        int new_len, int frame_size_fixed);
void rs_frame_buffer_process(struct rs_frame_buffer *buffer, int new_len,
                             uint8_t *sep, int sep_len);
void rs_frame_buffer_flush(struct rs_frame_buffer* buffer, int keep_n_frames);

/* sep-mode */
#define RS_APP_BUFFER_DEFAULT_SIZE 1000

struct rs_app_connection {
    rs_port_id_t port;

    /* frame configuration */
    uint8_t *frame_sep;
    uint8_t frame_sep_size;
    int frame_size_fixed; /* > 0 indicates fixed fame size mode */

    struct rs_frame_buffer buffer;

    struct rs_stat stat_in;
    struct rs_stat stat_skipped;

    int socket;
    struct sockaddr_in addr_server;

    int client_socket;
};




#endif
