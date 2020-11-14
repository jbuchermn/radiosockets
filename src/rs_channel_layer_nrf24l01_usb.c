#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include "rs_channel_layer_nrf24l01_usb.h"
#include "rs_server_state.h"
#include "rs_util.h"

static struct rs_channel_layer_vtable vtable;

#define CMD_SLEEP 10000

static void _cmd(int fd, const char *cmd) {
    write(fd, cmd, strlen(cmd));
    usleep(CMD_SLEEP);
}

static void _set_channel(struct rs_channel_layer_nrf24l01_usb *layer,
                         int n_channel) {
    if (n_channel == layer->on_channel)
        return;

    char cmd[200] = {0};
    sprintf(cmd, "AT+FREQ=2.%dG", (n_channel + 400));
    _cmd(layer->fd_serial, cmd);

    layer->on_channel = n_channel;
}

int rs_channel_layer_nrf24l01_usb_init(
    struct rs_channel_layer_nrf24l01_usb *layer, struct rs_server_state *server,
    uint8_t ch_base, config_setting_t *conf) {

    rs_channel_layer_init(&layer->super, server, ch_base, &vtable);

    const char *tty;
    if (config_setting_lookup_string(conf, "tty", &tty) != CONFIG_TRUE) {
        syslog(LOG_ERR, "Need to provide tty for nrf24l01_usb layer");
        return -1;
    }

    int initial_baud = 9600;
    config_setting_lookup_int(conf, "initial_baud", &initial_baud);

    /* open serial interface */
    layer->fd_serial = open(tty, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);

    if (layer->fd_serial < 0) {
        syslog(LOG_ERR, "Unable to open tty");
        return -1;
    }

    /* configure it */
    struct termios tty_conf;
    if (tcgetattr(layer->fd_serial, &tty_conf)) {
        syslog(LOG_ERR, "Unable to get tcattr");
        return -1;
    }

    // minimum interference
    cfmakeraw(&tty_conf);

    // speed
    int baud = B9600;
    switch (initial_baud) {
    case 4800:
        baud = B4800;
        break;
    case 9600:
        baud = B9600;
        break;
    case 19200:
        baud = B19200;
        break;
    case 57600:
        baud = B57600;
        break;
    case 115200:
        baud = B115200;
        break;
    default:
        syslog(LOG_ERR, "Unsupported initial Baud rate: %d, defaulting to 9600",
               initial_baud);
    }
    cfsetospeed(&tty_conf, baud);
    cfsetispeed(&tty_conf, baud);

    // control flags 8N1
    tty_conf.c_cflag = (tty_conf.c_cflag & ~CSIZE) | CS8;
    tty_conf.c_cflag &= ~PARENB;
    tty_conf.c_cflag &= ~CSTOPB;

    // lflags and oflags
    tty_conf.c_lflag = 0;
    tty_conf.c_oflag = 0;

    if (tcsetattr(layer->fd_serial, TCSANOW, &tty_conf)) {
        syslog(LOG_ERR, "Unable to set tcattr");
        return -1;
    }

    // Set Baud rate to 115200
    // 2 = 9600
    // 6 = 57600
    // 7 = 115200
    _cmd(layer->fd_serial, "AT+BAUD=7");

    // Update Baud rate
    if (tcgetattr(layer->fd_serial, &tty_conf)) {
        syslog(LOG_ERR, "Unable to get tcattr");
        return -1;
    }
    cfsetospeed(&tty_conf, B115200);
    cfsetispeed(&tty_conf, B115200);
    if (tcsetattr(layer->fd_serial, TCSANOW, &tty_conf) != 0) {
        syslog(LOG_ERR, "Unable to set tcattr");
        return -1;
    }


    /* Retrieve config to check connection */
    _cmd(layer->fd_serial, "AT?"); // Flush buf
    _cmd(layer->fd_serial, "AT?");

    /* if (buf[0] != 'O' || buf[1] != 'K') { */
    /*     printf("AT? %s\n", buf); */
    /*     syslog(LOG_ERR, "Cannot connect to nRF24L01 module on %s", tty); */
    /*     return -1; */
    /* } else { */
    /* } */

    /* Configure module */
    assert(sizeof(rs_server_id_t) == 2);
    char cmd[200] = {0};

    // Rate 250kbits (limited by chinese Baud)
    _cmd(layer->fd_serial, "AT+RATE=1");

    // Adresses
    sprintf(cmd, "AT+RXA=0xFF,0xFF,0xFF,0x%.2X,0x%.2X", (server->own_id >> 8),
            server->own_id & 0xFF);
    _cmd(layer->fd_serial, cmd);

    sprintf(cmd, "AT+TXA=0xFF,0xFF,0xFF,0x%.2X,0x%.2X", (server->other_id >> 8),
            server->other_id & 0xFF);
    _cmd(layer->fd_serial, cmd);

    // Initial channel
    layer->on_channel = -1;
    _set_channel(layer, 100);

    syslog(LOG_NOTICE, "Initialized nRF24L01 on %s", tty);

    layer->recv_buf_last_n = -1;

    return 0;
}

void rs_channel_layer_nrf24l01_usb_destroy(struct rs_channel_layer *super) {
    rs_channel_layer_base_destroy(super);

    struct rs_channel_layer_nrf24l01_usb *layer =
        rs_cast(rs_channel_layer_nrf24l01_usb, super);

    close(layer->fd_serial);
}

static int _ch_n(struct rs_channel_layer *super) { return 126; }

static int _transmit(struct rs_channel_layer *super, struct rs_packet *packet,
                     rs_channel_t channel) {
    
    struct rs_channel_layer_nrf24l01_usb *layer =
        rs_cast(rs_channel_layer_nrf24l01_usb, super);
    _set_channel(layer, rs_channel_layer_extract(&layer->super, channel));

    uint8_t tx_buf[RS_NRF24L01_TX_BUFSIZE];
    uint8_t *tx_ptr = tx_buf;
    int tx_len = RS_NRF24L01_TX_BUFSIZE;

    rs_packet_pack(packet, &tx_ptr, &tx_len);

    int n_packets = ceil((tx_ptr - tx_buf) / 29.0);
    for (int i = 0; i < n_packets; i++) {
        uint8_t packet[31];

        packet[0] = n_packets * 16 + i;
        memcpy(packet + 1, tx_buf + i * 29, 29);
        packet[30] = 0; // CRC
        write(layer->fd_serial, packet, 31);

        /* TODO Either make this unnecessary, or run each
         * channel layer in a separate thread */
        usleep(20000);
    }
    return tx_ptr - tx_buf;
}

static int _receive(struct rs_channel_layer *super,
                    struct rs_channel_layer_packet **result,
                    rs_channel_t channel) {

    struct rs_channel_layer_nrf24l01_usb *layer =
        rs_cast(rs_channel_layer_nrf24l01_usb, super);
    _set_channel(layer, rs_channel_layer_extract(&layer->super, channel));

    char res[32];
    int res_len = sizeof(res);
    int n;
    int n_res = 0;
    while ((n = read(layer->fd_serial, res + n_res, res_len - n_res)) > 0) {
        n_res += n;

        if (n_res == 31) {
            int n_packets = res[0] / 16;
            int packet = res[0] % 16;

            if (packet != layer->recv_buf_last_n && packet != 0) {
                /* Missed packets, skipping */
                layer->recv_buf_last_n = -1;
            } else {
                memcpy(layer->recv_buf + 29 * packet, res + 1, 29);
                layer->recv_buf_last_n = packet;

                if (packet == n_packets - 1) {
                    /* recv_buf contains one full packet */
                    struct rs_channel_layer_packet *unpacked =
                        calloc(1, sizeof(struct rs_channel_layer_packet));

                    if (rs_channel_layer_packet_unpack(unpacked, NULL,
                                                       layer->recv_buf,
                                                       n_packets*29)) {
                        syslog(LOG_DEBUG,
                               "Received packet which could not be unpacked on "
                               "channel "
                               "layer (%d fragments)",
                               n_packets);
                        rs_packet_destroy(&unpacked->super);
                        free(unpacked);
                        return RS_CHANNEL_LAYER_IRR;
                    }

                    (*result) = unpacked;
                    return 0;
                }
            }
            n_res = 0;
        }
    }

    return -1;
}

static struct rs_channel_layer_vtable vtable = {
    .destroy = rs_channel_layer_nrf24l01_usb_destroy,
    ._transmit = _transmit,
    ._receive = _receive,
    .ch_n = _ch_n,
};
