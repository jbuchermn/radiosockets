#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <linux/if.h>
#include <linux/nl80211.h>
#include <sys/ioctl.h>

#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>

#include "radiotap-library/platform.h"
#include "radiotap-library/radiotap_iter.h"

#include "rs_channel_layer_pcap.h"
#include "rs_channel_layer_pcap_packet.h"
#include "rs_packet.h"
#include "rs_server_state.h"
#include "rs_util.h"

/*
 ************************************************************************
 * nl80211 code
 */
struct nl_command {
    struct rs_channel_layer_pcap *root;

    int cb_in_progress;
    struct nl_msg *msg;

    void *ret;
};

static int callback_finish(struct nl_msg *msg, void *arg) {
    struct nl_command *cmd = arg;

    cmd->cb_in_progress = 0;
    return NL_OK;
}

static int callback_error(struct sockaddr_nl *sock, struct nlmsgerr *msg,
                          void *arg) {
    struct nl_command *cmd = arg;

    cmd->cb_in_progress = 0;
    return NL_OK;
}

static int nl_command_init(struct nl_command *command,
                           struct rs_channel_layer_pcap *root, int cmd,
                           int flags, nl_recvmsg_msg_cb_t callback) {
    command->root = root;
    command->ret = NULL;

    nl_cb_set(root->nl_cb, NL_CB_FINISH, NL_CB_CUSTOM, callback_finish,
              command);
    nl_cb_set(root->nl_cb, NL_CB_ACK, NL_CB_CUSTOM, callback_finish, command);
    nl_cb_set(root->nl_cb, NL_CB_VALID, NL_CB_CUSTOM, callback, command);
    /* nl_cb_err(root->nl_cb, NL_CB_VERBOSE, NULL, stderr); */
    nl_cb_err(root->nl_cb, NL_CB_CUSTOM, callback_error, command);

    command->msg = nlmsg_alloc();
    if (!command->msg) {
        syslog(LOG_ERR, "Failed to allocate netlink message");
        return -2;
    }
    genlmsg_put(command->msg, NL_AUTO_PORT, NL_AUTO_SEQ, root->nl_id, 0, flags,
                cmd, 0);
    return 0;
}

static int nl_command_run(struct nl_command *command) {
    nl_send_auto_complete(command->root->nl_socket, command->msg);
    command->cb_in_progress = 1;
    while (command->cb_in_progress) {
        nl_recvmsgs(command->root->nl_socket, command->root->nl_cb);
    }
    nlmsg_free(command->msg);

    return 0;
}

/* Callback for NL80211_CMD_GET_WIPHY */
struct nl_cb_get_wiphy_ret {
    int n_capable_phys;
    uint32_t capable_phys[20];
};

static int nl_cb_get_wiphy(struct nl_msg *msg, void *arg) {
    struct nl_command *cmd = arg;
    struct nl_cb_get_wiphy_ret *ret = cmd->ret;
    /* nl_msg_dump(msg, stdout); */

    struct genlmsghdr *genlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(genlh, 0),
              genlmsg_attrlen(genlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES]) {

        int rem_mode;
        struct nlattr *nl_mode;
        nla_for_each_nested(nl_mode, tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES],
                            rem_mode) {
            if (nla_type(nl_mode) == NL80211_IFTYPE_MONITOR) {
                if (tb_msg[NL80211_ATTR_WIPHY]) {
                    ret->capable_phys[ret->n_capable_phys] =
                        nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
                    ret->n_capable_phys++;
                }
            }
        }
    }

    return NL_OK;
}

/* Callback for NL80211_CMD_GET_INTERFACE */
struct nl_cb_get_interface_ret {
    int exists;
    uint32_t other_interfaces[20];
    int n_other_interfaces;
};

static int nl_cb_get_interface(struct nl_msg *msg, void *arg) {
    struct nl_command *cmd = arg;
    struct nl_cb_get_interface_ret *ret = cmd->ret;
    /* nl_msg_dump(msg, stdout); */

    struct genlmsghdr *genlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(genlh, 0),
              genlmsg_attrlen(genlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_WIPHY]) {
        if (cmd->root->nl_wiphy == nla_get_u32(tb_msg[NL80211_ATTR_WIPHY])) {
            if (tb_msg[NL80211_ATTR_IFNAME]) {
                if (!strcmp(cmd->root->nl_ifname,
                            nla_get_string(tb_msg[NL80211_ATTR_IFNAME]))) {
                    ret->exists = 1;
                    if (tb_msg[NL80211_ATTR_IFINDEX]) {
                        cmd->root->nl_if =
                            nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
                    }
                } else {
                    if (tb_msg[NL80211_ATTR_IFINDEX]) {
                        ret->other_interfaces[ret->n_other_interfaces] =
                            nla_get_u32(tb_msg[NL80211_ATTR_IFINDEX]);
                        ret->n_other_interfaces++;
                    }
                }
            }
        }
    }

    return NL_OK;
}

/* Callback for NL80211_CMD_NEW_INTERFACE */
static int nl_cb_new_interface(struct nl_msg *msg, void *arg) {
    struct nl_command *cmd = arg;
    /* nl_msg_dump(msg, stdout); */

    struct genlmsghdr *genlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(genlh, 0),
              genlmsg_attrlen(genlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_IFINDEX]) {
        cmd->root->nl_if = nla_get_u32(tb_msg[NL80211_ATTR_IFNAME]);
    }
    return NL_OK;
}

/* Default callback */
static int nl_cb_default(struct nl_msg *msg, void *arg) { return NL_OK; }

static void nl_set_channel(struct rs_channel_layer_pcap *layer,
                           uint16_t channel) {
    if (channel == layer->on_channel)
        return;

    struct nl_command cmd;
    nl_command_init(&cmd, layer, NL80211_CMD_SET_WIPHY, 0, nl_cb_default);
    nla_put_u32(cmd.msg, NL80211_ATTR_WIPHY, layer->nl_wiphy);

    nla_put_u32(cmd.msg, NL80211_ATTR_WIPHY_FREQ, 2412 + 5 * channel);
    nla_put_u32(cmd.msg, NL80211_ATTR_CHANNEL_WIDTH, NL80211_CHAN_WIDTH_20);
    nla_put_u32(cmd.msg, NL80211_ATTR_WIPHY_CHANNEL_TYPE, NL80211_CHAN_HT20);
    /* nla_put_u32(cmd.msg, NL80211_ATTR_CENTER_FREQ1, 2412 + 5 * channel); */
    /* nla_put_u32(cmd.msg, NL80211_ATTR_CENTER_FREQ2, 2412 + 5 * channel); */

    nl_command_run(&cmd);

    layer->on_channel = channel;
}

/*
 ************************************************************************
 * setup/shutdown code
 */
static struct rs_channel_layer_vtable vtable;

int rs_channel_layer_pcap_init(struct rs_channel_layer_pcap *layer,
                               struct rs_server_state *server, int phys,
                               char *ifname) {
    rs_channel_layer_init(&layer->super, server);
    layer->super.vtable = &vtable;
    layer->pcap = NULL;

    /* initialize nl80211 */
    layer->nl_socket = nl_socket_alloc();
    if (!layer->nl_socket) {
        syslog(LOG_ERR, "Failed to allocate netlink socket");
        return -1;
    }

    nl_socket_set_buffer_size(layer->nl_socket, 8192, 8192);

    if (genl_connect(layer->nl_socket)) {
        syslog(LOG_ERR, "Failed to connect to netlink socket");
        nl_close(layer->nl_socket);
        nl_socket_free(layer->nl_socket);
        return -1;
    }

    layer->nl_id = genl_ctrl_resolve(layer->nl_socket, "nl80211");
    if (layer->nl_id < 0) {
        syslog(LOG_ERR, "nl80211 interface not found");
        nl_close(layer->nl_socket);
        nl_socket_free(layer->nl_socket);
        return -1;
    }

    layer->nl_cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!layer->nl_cb) {
        syslog(LOG_ERR, "Failed to allocate netlink callback");
        nl_close(layer->nl_socket);
        nl_socket_free(layer->nl_socket);
        return -1;
    }

    /* look for monitor-capable devices and store in nl_wiphy, nl_ifname */
    struct nl_command cmd;
    struct nl_cb_get_wiphy_ret ret = {0};
    nl_command_init(&cmd, layer, NL80211_CMD_GET_WIPHY, NLM_F_DUMP,
                    nl_cb_get_wiphy);
    cmd.ret = &ret;
    nl_command_run(&cmd);

    if (ret.n_capable_phys == 0) {
        syslog(LOG_ERR, "No monitor-capable devices available");
        rs_channel_layer_destroy(&layer->super);
        return -2;
    } else {
        if (phys == -1) {
            layer->nl_wiphy = ret.capable_phys[0];
        } else {
            int is_ok = 0;
            for (int i = 0; i < ret.n_capable_phys; i++) {
                if (ret.capable_phys[i] == phys) {
                    is_ok = 1;
                    break;
                }
            }
            if (!is_ok) {
                syslog(LOG_ERR, "phys#%d does not exist", phys);
                rs_channel_layer_destroy(&layer->super);
                return -2;
            }
            layer->nl_wiphy = phys;
        }
    }
    strcpy(layer->nl_ifname, ifname);
    syslog(LOG_NOTICE, "Using physical device #%d ifname %s", layer->nl_wiphy,
           layer->nl_ifname);

    /* see if interface exists already */
    nl_command_init(&cmd, layer, NL80211_CMD_GET_INTERFACE, NLM_F_DUMP,
                    nl_cb_get_interface);

    struct nl_cb_get_interface_ret ret1 = {0};
    cmd.ret = &ret1;
    nl_command_run(&cmd);
    if (!ret1.exists) {
        syslog(LOG_NOTICE,
               "Interface '%s' does not yet exist, creating it...\n",
               layer->nl_ifname);

        /* create monitor interface */
        nl_command_init(&cmd, layer, NL80211_CMD_NEW_INTERFACE, 0,
                        nl_cb_new_interface);
        nla_put_u32(cmd.msg, NL80211_ATTR_WIPHY, layer->nl_wiphy);
        nla_put_string(cmd.msg, NL80211_ATTR_IFNAME, layer->nl_ifname);
        nla_put_u32(cmd.msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
        nl_command_run(&cmd);

        syslog(LOG_NOTICE, "...done\n");
    } else {
        syslog(LOG_NOTICE,
               "Interface '%s' exists, putting it in monitor mode...",
               layer->nl_ifname);

        /*
         * Possibly busy - need custom error handler otherwise
         * program stalls
         */
        /* put it in monitor mode */
        nl_command_init(&cmd, layer, NL80211_CMD_SET_INTERFACE, 0,
                        nl_cb_default);
        nla_put_u32(cmd.msg, NL80211_ATTR_IFINDEX, layer->nl_if);
        nla_put_u32(cmd.msg, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_MONITOR);
        nl_command_run(&cmd);
        syslog(LOG_NOTICE, "...done\n");
    }

    /* try and remove other interfaces */
    for (int i = 0; i < ret1.n_other_interfaces; i++) {
        syslog(LOG_NOTICE, "Deleting unused interface %d...\n",
               ret1.other_interfaces[i]);

        nl_command_init(&cmd, layer, NL80211_CMD_DEL_INTERFACE, 0,
                        nl_cb_default);
        nla_put_u32(cmd.msg, NL80211_ATTR_IFINDEX, ret1.other_interfaces[i]);
        nl_command_run(&cmd);
        syslog(LOG_NOTICE, "...done\n");
    }

    /* ip link set ifname up */
    struct ifreq ifr;
    int fd;

    if ((fd = socket(PF_PACKET, SOCK_DGRAM, 0)) < 0) {
        syslog(LOG_ERR, "Could not open up socket");
        return -1;
    }

    strncpy(ifr.ifr_name, layer->nl_ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr)) {
        syslog(LOG_ERR, "SIOCGIFFLAGS");
        close(fd);
        return -1;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr)) {
        syslog(LOG_ERR, "SIOCGIFFLAGS");
        close(fd);
        return -1;
    }
    close(fd);

    /* initialize pcap */
    char errbuf[PCAP_ERRBUF_SIZE];
    layer->pcap = pcap_create(layer->nl_ifname, errbuf);
    pcap_set_snaplen(layer->pcap, -1);
    pcap_set_timeout(layer->pcap, -1);

    int err;
    if ((err = pcap_activate(layer->pcap)) != 0) {
        syslog(LOG_ERR, "PCAP activate failed: %d\n", err);
        return -1;
    }

    if (pcap_setnonblock(layer->pcap, 1, errbuf) != 0) {
        syslog(LOG_ERR, "PCAP setnonblock failed: %s\n", errbuf);
        return -1;
    }

    /* set filter */
    /* assert(sizeof(rs_server_id_t) == 2); */
    assert(sizeof(rs_server_id_t) == 1);
    int link_encap = pcap_datalink(layer->pcap);
    struct bpf_program bpfprogram;
    char program[200];

    switch (link_encap) {
    case DLT_IEEE802_11_RADIO:
        sprintf(program, "ether src %.12llx && ether dst %.12llx",
                (unsigned long long)0x112233445500 |
                    /* ((layer->super.server->other_id >> 8) << 8) | */
                    (layer->super.server->other_id & 0xFF),
                (unsigned long long)0x112233445500 |
                    /* ((layer->super.server->own_id >> 8) << 8) | */
                    (layer->super.server->own_id & 0xFF));
        break;

    default:
        syslog(LOG_ERR, "Unknown encapsulation");
        return -1;
    }

    if (pcap_compile(layer->pcap, &bpfprogram, program, 1, 0) == -1) {
        syslog(LOG_ERR, "Unable to compile %s: %s", program,
               pcap_geterr(layer->pcap));
    }

    if (pcap_setfilter(layer->pcap, &bpfprogram) == -1) {
        syslog(LOG_ERR, "Unable to set filter %s: %s", program,
               pcap_geterr(layer->pcap));
    }

    pcap_freecode(&bpfprogram);

    /* set initial channel */
    layer->on_channel = 0;
    nl_set_channel(layer, 0x06);

    return 0;
}

void rs_channel_layer_pcap_destroy(struct rs_channel_layer *super) {
    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);
    if (layer->pcap)
        pcap_close(layer->pcap);
    nl_cb_put(layer->nl_cb);
    nl_close(layer->nl_socket);
    nl_socket_free(layer->nl_socket);
}

/*
 ************************************************************************
 * tx/rx code
 */
static uint8_t tx_radiotap_header[] __attribute__((unused)) = {
    0x00, // it_version
    0x00, // it_pad

    0x11,
    0x00, // it_len

    // it_present
    // bits 7-0, 15-8, 23-16, 31-24
    // set CHANNEL: bit 3
    // set TX_FLAGS: bit 15
    // set MCS: bit 19
    0x08, 0x80, 0x08, 0x00,

    // CHANNEL
    // u16 frequency (MHz), u16 flags
    // frequency: to be set
    // flags 7-0, 15-8
    // set Dynamic CCK-OFDM channel: bit 10
    // set 2GHz channel: bit 7
    0x00, 0x00, 0x80, 0x04,

    // TX_FLAGS
    // u16 flags 7-0, 15-8
    // set NO_ACK: bit 3
    0x08, 0x00,

    // MCS
    // u8 known, u8 flags, u8 mcs
    // known: Guard Interval, Bandwidth, MCS, STBC, FEC
    // flags: _xx1_gbb, xx: STBC, g: GI, bb: bandwidth
    (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW |
     IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC |
     IEEE80211_RADIOTAP_MCS_HAVE_FEC),
    0x10, 0x00};

static uint8_t ieee80211_header[] __attribute__((unused)) = {
    // IEEE802.11 header
    0x08,
    0x01,
    0x00,
    0x00,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    // src mac
    0x11,
    0x22,
    0x33,
    0x44,
    0x55,
    0x66,
    // dst mac
    0x11,
    0x22,
    0x33,
    0x44,
    0x55,
    0x66,
    0x00,
    0x00,
};

int rs_channel_layer_pcap_transmit(struct rs_channel_layer *super,
                                   struct rs_packet *packet,
                                   rs_channel_t channel) {
    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);

    if (!rs_channel_layer_owns_channel(super, channel)) {
        syslog(LOG_ERR, "Attempting to send packet through wrong channel");
        return -1;
    }
    nl_set_channel(layer, rs_channel_layer_extract(&layer->super, channel));

    uint8_t tx_buf[RS_PCAP_TX_BUFSIZE];
    uint8_t *tx_ptr = tx_buf;
    int tx_len = RS_PCAP_TX_BUFSIZE;

    memcpy(tx_ptr, tx_radiotap_header, sizeof(tx_radiotap_header));

    // Set frequency (not clear if necessary)
    uint16_t freq = 2412 + 5 * layer->on_channel;
    tx_ptr[8] = (uint8_t)freq;
    tx_ptr[9] = (uint8_t)(freq >> 8);

    // Set MCS
    // https://en.wikipedia.org/wiki/IEEE_802.11n-2009#Data_rates
    tx_ptr[15] |= IEEE80211_RADIOTAP_MCS_BW_20;
    /* tx_ptr[15] |= IEEE80211_RADIOTAP_MCS_SGI; */
    tx_ptr[16] = 0;

    tx_ptr += sizeof(tx_radiotap_header);
    tx_len -= sizeof(tx_radiotap_header);

    memcpy(tx_ptr, ieee80211_header, sizeof(ieee80211_header));

    /* assert(sizeof(rs_server_id_t) == 2); */
    assert(sizeof(rs_server_id_t) == 1);
    // src mac
    /* tx_ptr[14] = layer->super.server->own_id >> 8; */
    tx_ptr[15] = layer->super.server->own_id & 0xFF;
    // dst mac
    /* tx_ptr[20] = layer->super.server->other_id >> 8; */
    tx_ptr[21] = layer->super.server->other_id & 0xFF;

    tx_ptr += sizeof(ieee80211_header);
    tx_len -= sizeof(ieee80211_header);

    struct rs_channel_layer_pcap_packet tmp_packet;
    rs_channel_layer_pcap_packet_init(&tmp_packet, NULL, packet, NULL, 0,
                                      channel);
    uint8_t *tx_begin_payload = tx_ptr;
    rs_packet_pack(&tmp_packet.super, &tx_ptr, &tx_len);
    rs_packet_destroy(&tmp_packet.super);

    if (pcap_inject(layer->pcap, tx_buf, tx_ptr - tx_buf) != tx_ptr - tx_buf) {
        syslog(LOG_ERR, "Could not inject packet");
        return -1;
    }

    return tx_ptr - tx_begin_payload - RS_CHANNEL_LAYER_PCAP_HEADER_SIZE;
}

static int rs_channel_layer_pcap_receive(struct rs_channel_layer *super,
                                         struct rs_packet **packet,
                                         rs_channel_t *channel) {

    struct rs_channel_layer_pcap *layer = rs_cast(rs_channel_layer_pcap, super);
    if (layer->pcap == NULL) {
        return -1;
    }
    if (*channel) {
        nl_set_channel(layer, rs_channel_layer_extract(&layer->super, *channel));
    }

    struct pcap_pkthdr header;
    const uint8_t *radiotap_header = pcap_next(layer->pcap, &header);

    if (radiotap_header) {
        struct ieee80211_radiotap_iterator it;
        int status = ieee80211_radiotap_iterator_init(
            &it, (struct ieee80211_radiotap_header *)radiotap_header,
            header.caplen, NULL);

        int flags = -1;
        int mcs_known = -1;
        int mcs_flags = -1;
        int mcs = -1;
        int rate = -1;
        /* int chan = -1; */
        /* int chan_flags = -1; */
        /* int antenna = -1; */

        while (status == 0) {
            if ((status = ieee80211_radiotap_iterator_next(&it)))
                continue;

            switch (it.this_arg_index) {
            case IEEE80211_RADIOTAP_FLAGS:
                flags = *(uint8_t *)(it.this_arg);
                break;
            case IEEE80211_RADIOTAP_MCS:
                mcs_known = *(uint8_t *)(it.this_arg);
                mcs_flags = *(((uint8_t *)(it.this_arg)) + 1);
                mcs = *(((uint8_t *)(it.this_arg)) + 2);
                break;
            case IEEE80211_RADIOTAP_RATE:
                rate = *(uint8_t *)(it.this_arg);
                break;
                /* case IEEE80211_RADIOTAP_CHANNEL: */
                /*     chan = get_unaligned((uint16_t *)(it.this_arg)); */
                /*     chan_flags = get_unaligned(((uint16_t *)(it.this_arg)) +
                 * 1); */
                /*     break; */
                /* case IEEE80211_RADIOTAP_ANTENNA: */
                /*     antenna = *(uint8_t *)(it.this_arg); */
                /*     break; */
                /* default: */
                /*     break; */
            }
        }

        /* syslog(LOG_DEBUG, "rate: %d known %02x flags %02x mcs %d", rate, */
        /*        mcs_known, mcs_flags, mcs); */
        if (flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_BADFCS)) {
            syslog(LOG_DEBUG, "Received bad FCS packet");
            return RS_CHANNEL_LAYER_BADFCS;
        }

        const uint8_t *payload = radiotap_header + it._max_length;
        int payload_len = header.caplen - it._max_length;
        if (flags >= 0 && (((uint8_t)flags) & IEEE80211_RADIOTAP_F_FCS)) {
            payload_len -= 4;
        }

        payload += sizeof(ieee80211_header);
        payload_len -= sizeof(ieee80211_header);

        if (payload_len < 0)
            return RS_CHANNEL_LAYER_EOF;

        uint8_t *payload_copy = calloc(payload_len, sizeof(uint8_t));
        memcpy(payload_copy, payload, payload_len);

        struct rs_channel_layer_pcap_packet pcap_packet;
        if (rs_channel_layer_pcap_packet_unpack(&pcap_packet, payload_copy,
                                                payload_copy, payload_len)) {
            syslog(LOG_DEBUG,
                   "Received packet which could not be unpacked on channel "
                   "layer (%db)",
                   payload_len);
            free(payload_copy);
            return RS_CHANNEL_LAYER_IRR;
        }

        if (!rs_channel_layer_owns_channel(&layer->super,
                                           pcap_packet.channel)) {
            syslog(LOG_DEBUG, "Received packet on channel without ownership");
            free(payload_copy);
            return RS_CHANNEL_LAYER_IRR;
        }

        *packet = calloc(1, sizeof(struct rs_packet));
        rs_packet_init(*packet, pcap_packet.super.payload_ownership,
                       pcap_packet.super.payload_packet,
                       pcap_packet.super.payload_data,
                       pcap_packet.super.payload_data_len);
        *channel = pcap_packet.channel;

        return 0;
    }

    return RS_CHANNEL_LAYER_EOF;
}

uint8_t rs_channel_layer_pcap_ch_base(struct rs_channel_layer *super) {
    return 0x01;
}

int rs_channel_layer_pcap_ch_n(struct rs_channel_layer *super) { return 12; }

static struct rs_channel_layer_vtable vtable = {
    .destroy = rs_channel_layer_pcap_destroy,
    .transmit = rs_channel_layer_pcap_transmit,
    .receive = rs_channel_layer_pcap_receive,
    .ch_base = rs_channel_layer_pcap_ch_base,
    .ch_n = rs_channel_layer_pcap_ch_n,
};
