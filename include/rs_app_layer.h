#ifndef RS_APP_LAYER_H
#define RS_APP_LAYER_H

#include <arpa/inet.h>

#include "rs_port_layer.h"

struct rs_server_state;
struct rs_app_connection;

struct rs_app_layer {
    struct rs_server_state *server;

    struct rs_app_connection **connections;
    int n_connections;
};

void rs_app_layer_init(struct rs_app_layer *layer,
                       struct rs_server_state *server);

void rs_app_layer_open_connection(struct rs_app_layer *layer, rs_port_id_t port,
                                  int udp_port, int frame_size);
void rs_app_layer_destroy(struct rs_app_layer *layer);
void rs_app_connection_destroy(struct rs_app_connection *connection);

void rs_app_layer_main(struct rs_app_layer *layer, struct rs_packet *received,
                       rs_port_id_t received_port);

struct rs_app_connection {
    rs_port_id_t port;

    uint8_t* frame_buffer;
    uint8_t* frame_buffer_at;
    int frame_size;

    int socket;
    struct sockaddr_in addr_server;

    int addr_client_len; /* > 0 indicates there is a connection */
    struct sockaddr_in addr_client;
};

#endif