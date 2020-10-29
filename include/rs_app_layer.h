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

/* Maximum supported frame size (in bytes) in sep-mode */
#define RS_APP_BUFFER_SIZE 500000

struct rs_app_connection {
    rs_port_id_t port;

    /* frame configuration */
    uint8_t *frame_sep;
    uint8_t frame_sep_size;
    int frame_size_fixed; /* > 0 indicates fixed fame size mode */

    /* buffering */
    uint8_t *buffer;
    int buffer_at;
    int buffer_size;

    struct rs_stat stat;

    int socket;
    struct sockaddr_in addr_server;

    int client_socket;
};

#endif
