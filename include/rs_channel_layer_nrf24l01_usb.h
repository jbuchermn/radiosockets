#ifndef RS_CHANNEL_LAYER_NRF24L01_USB_H
#define RS_CHANNEL_LAYER_NRF24L01_USB_H

#include "rs_channel_layer.h"
#include <libconfig.h>

#define RS_NRF24L01_TX_BUFSIZE (29*16)

struct rs_channel_layer_nrf24l01_usb {
    struct rs_channel_layer super;

    int fd_serial;

    int on_channel;

    int recv_buf_last_n;
    uint8_t recv_buf[29 * 16];
};

int rs_channel_layer_nrf24l01_usb_init(
    struct rs_channel_layer_nrf24l01_usb *layer, struct rs_server_state *server,
    uint8_t ch_base, config_setting_t *conf);

void rs_channel_layer_nrf24l01_usb_destroy(
    struct rs_channel_layer *layer);

#endif
