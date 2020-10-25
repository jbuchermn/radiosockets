#ifndef RS_STAT_H
#define RS_STAT_H

#include <time.h>

#define RS_STAT_N 10
#define RS_STAT_DT_MSEC 500

struct rs_stat {
    struct timespec stat_t0;
    enum {
        RS_STAT_AGG_SUM,
        RS_STAT_AGG_AVG,
        RS_STAT_AGG_COUNT,
    } aggregate;

    // [................d....................]
    // [...last_data...]|[data / n_data...000]
    //                  t0
    double d[2 * RS_STAT_N];
    double *last_data;
    struct timespec t0;
    double *data;
    int n_data[RS_STAT_N];

    double norm_factor;
    const char *title;
    const char *unit;
};

void rs_stat_init(struct rs_stat *stat, int aggregate, const char *title,
                  const char *unit, double norm_factor);
void rs_stat_register(struct rs_stat *stat, double value);
void rs_stat_flush(struct rs_stat *stat);

void rs_stat_printf(struct rs_stat *stat);
double rs_stat_current(struct rs_stat *stat);

struct rs_stats {
    struct rs_stat tx_stat_bits;
    struct rs_stat tx_stat_packets;

    struct rs_stat rx_stat_bits;
    struct rs_stat rx_stat_packets;
    struct rs_stat rx_stat_missed;
    struct rs_stat rx_stat_dt;

    struct rs_stat other_rx_stat_bits;
    struct rs_stat other_rx_stat_packets;
    struct rs_stat other_rx_stat_missed;
    struct rs_stat other_rx_stat_dt;
};

/* TODO stats methods */
//
// **** from old rs_port_layer init method ***
// rs_stat_init(&layer->infos[i][j].tx_stat_bits, RS_STAT_AGG_SUM,
//              "TX", "bps", 1000. / RS_STAT_DT_MSEC);
// rs_stat_init(&layer->infos[i][j].tx_stat_packets, RS_STAT_AGG_COUNT,
//              "TX", "pps", 1000. / RS_STAT_DT_MSEC);
//
// rs_stat_init(&layer->infos[i][j].rx_stat_bits, RS_STAT_AGG_SUM,
//              "RX", "bps", 1000. / RS_STAT_DT_MSEC);
// rs_stat_init(&layer->infos[i][j].rx_stat_packets, RS_STAT_AGG_COUNT,
//              "RX", "pps", 1000. / RS_STAT_DT_MSEC);
// rs_stat_init(&layer->infos[i][j].rx_stat_dt, RS_STAT_AGG_AVG,
//              "RX dt", "ms", 1.);
// rs_stat_init(&layer->infos[i][j].rx_stat_missed, RS_STAT_AGG_AVG,
//              "RX miss", "", 1.);
//
// rs_stat_init(&layer->infos[i][j].other_rx_stat_bits,
//              RS_STAT_AGG_AVG, "-RX", "bps", 1.);
// rs_stat_init(&layer->infos[i][j].other_rx_stat_dt, RS_STAT_AGG_AVG,
//              "-RX dt", "ms", 1.);
// rs_stat_init(&layer->infos[i][j].other_rx_stat_missed,
//              RS_STAT_AGG_AVG, "-RX miss", "", 1.);
//

// **** from old rs_port_layer receive method ***
//
// struct timespec now;
// clock_gettime(CLOCK_REALTIME, &now);
// uint64_t now_nsec =
//     now.tv_sec * (uint64_t)1000000000L + now.tv_nsec;
// double dt_msec = (now_nsec - unpacked.ts_sent) / 1000000L;
// rs_stat_register(&info->rx_stat_packets, 1.0);
// rs_stat_register(&info->rx_stat_bits, 8 * bytes);
// rs_stat_register(&info->rx_stat_dt, dt_msec);
// for (int i = 0; i < unpacked.seq - info->rx_last_seq - 1; i++)
//     rs_stat_register(&info->rx_stat_missed, 1.0);
// rs_stat_register(&info->rx_stat_missed, 0.0);
//
// info->rx_last_ts = now;
// info->rx_last_seq = unpacked.seq;
//
// rs_stat_register(&info->other_rx_stat_bits,
//                  (double)unpacked.rx_bitrate);
// rs_stat_register(&info->other_rx_stat_missed,
//                  0.0001 * (double)unpacked.rx_missed);
// rs_stat_register(&info->other_rx_stat_dt, (double)unpacked.rx_dt);

#endif
