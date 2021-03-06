#ifndef RS_STAT_H
#define RS_STAT_H

#include <stdint.h>
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
    struct rs_stat tx_stat_bits_packet_size;
    struct rs_stat tx_stat_errors;

    struct rs_stat rx_stat_bits;
    struct rs_stat rx_stat_bits_packet_size;
    struct rs_stat rx_stat_missed;

    struct rs_stat other_tx_stat_bits;
    struct rs_stat other_rx_stat_bits;
    struct rs_stat other_rx_stat_missed;
};

#define RS_STATS_PLACE_N 9
void rs_stats_place(struct rs_stats *stats, double *into);

struct rs_stats_packed;

void rs_stats_init(struct rs_stats *stats);
void rs_stats_register_tx(struct rs_stats *stats, int bytes);
void rs_stats_register_rx(struct rs_stats *stats, int bytes,
                          int missed_packets);
void rs_stats_register_published_stats(struct rs_stats *stats,
                                       struct rs_stats_packed *received_stats);
void rs_stats_printf(struct rs_stats *stats);

struct rs_stats_packed {
    uint16_t tx_bits;   /* bitrate in kbps */
    uint16_t rx_bits;   /* bitrate in kbps */
    uint16_t rx_missed; /* normalized to 10000 */
};

void rs_stats_packed_init(struct rs_stats_packed *packed,
                          struct rs_stats *from);
int rs_stats_packed_len(struct rs_stats_packed *packed);
int rs_stats_packed_pack(struct rs_stats_packed *packed, uint8_t **buffer,
                         int *buffer_len);
int rs_stats_packed_unpack(struct rs_stats_packed *unpacked, uint8_t **buffer,
                           int *buffer_len);

#endif
