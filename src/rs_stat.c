#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "rs_stat.h"
#include "rs_util.h"

void rs_stat_init(struct rs_stat *stat, int aggregate, const char *title,
                  const char *unit, double norm_factor) {
    stat->last_data = stat->d;
    stat->data = stat->d + RS_STAT_N;

    memset(stat->last_data, 0.0, RS_STAT_N * sizeof(double));
    memset(stat->data, 0.0, RS_STAT_N * sizeof(double));
    memset(stat->n_data, 0, RS_STAT_N * sizeof(int));

    clock_gettime(CLOCK_REALTIME, &stat->t0);
    stat->t0.tv_nsec %= 1000000L;
    stat->aggregate = aggregate;
    stat->title = title;
    stat->unit = unit;
    stat->norm_factor = norm_factor;
}

void rs_stat_register(struct rs_stat *stat, double value) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long millis = msec_diff(now, stat->t0);
    int idx = millis / RS_STAT_DT_MSEC;
    if (idx < 0) {
        syslog(LOG_ERR, "rs_stat_register: unexpected timestamp");
        return;
    }
    if (idx >= RS_STAT_N) {
        rs_stat_flush(stat);
        rs_stat_register(stat, value);
    } else {
        switch (stat->aggregate) {
        case RS_STAT_AGG_SUM:
            stat->data[idx] += value;
            break;
        case RS_STAT_AGG_AVG:
            stat->data[idx] *= stat->n_data[idx];
            stat->data[idx] += value;
            stat->data[idx] /= stat->n_data[idx] + 1;
            break;
        case RS_STAT_AGG_COUNT:
            stat->data[idx] += 1.0;
            break;
        default:
            syslog(LOG_ERR, "rs_stat_register: unknown aggregation method");
        }
        stat->n_data[idx]++;
    }
}

void rs_stat_flush(struct rs_stat *stat) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long millis = msec_diff(now, stat->t0);
    int idx = millis / RS_STAT_DT_MSEC;
    if (idx < 0) {
        syslog(LOG_ERR, "rs_stat_register: unexpected timestamp");
        return;
    }
    if (idx < RS_STAT_N)
        return;

    /*
     * N = 5
     * - idx = 5
     *      xxxxx|yyyyy|    ==> yyyyy|00000
     *           t0    t           t=t0
     * - idx = 8 e.g.
     *      xxxxx|yyyyy???| ==> yy000|00000
     *           t0       t        t=t0
     * - idx >= 10
     *      xxxxx|????????| ==> 00000|00000
     *           t0       t        t=t0
     */
    memset(stat->last_data, 0.0, RS_STAT_N * sizeof(double));
    for (int i = idx - RS_STAT_N, j = 0; i < RS_STAT_N; i++, j++) {
        stat->last_data[j] = stat->data[i];
    }
    memset(stat->data, 0.0, RS_STAT_N * sizeof(double));
    memset(stat->n_data, 0, RS_STAT_N * sizeof(int));

    long t0_msec = stat->t0.tv_sec * 1000L + stat->t0.tv_nsec / 1000000L;
    t0_msec += idx * RS_STAT_DT_MSEC;

    stat->t0.tv_sec = t0_msec / 1000L;
    stat->t0.tv_nsec = (t0_msec % 1000L) * 1000000L;
}

static void printf_val(double val) {
    const char *neg = " ";
    if (val < 0) {
        val *= -1;
        neg = "-";
    }

    int m = 0;
    while (val > 1000) {
        val /= 1000;
        m += 1;
    }

    while (val < 1 && val != 0 /* D'oh */) {
        val *= 1000;
        m -= 1;
    }

    const char *ms = " ";
    switch (m) {
    case -3:
        ms = "n";
        break;
    case -2:
        ms = "u";
        break;
    case -1:
        ms = "m";
        break;
    case 0:
        ms = " ";
        break;
    case 1:
        ms = "k";
        break;
    case 2:
        ms = "M";
        break;
    case 3:
        ms = "G";
        break;
    default:
        ms = "?";
        break;
    }

    printf("%s%3d%s", neg, (int)val, ms);
}

void rs_stat_printf(struct rs_stat *stat) {
    rs_stat_flush(stat);

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long millis = msec_diff(now, stat->t0);
    int idx = millis / RS_STAT_DT_MSEC;

    printf("STAT[%10s]: ", stat->title);
    for (int i = 0; i < RS_STAT_N - idx; i++) {
        printf_val(stat->last_data[i] * stat->norm_factor);
        printf("%-3s ", stat->unit);
    }
    for (int i = 0; i < idx; i++) {
        printf_val(stat->data[i] * stat->norm_factor);
        printf("%-3s ", stat->unit);
    }

    printf("\n");
}

double rs_stat_current(struct rs_stat *stat) {
    rs_stat_flush(stat);

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long millis = msec_diff(now, stat->t0);
    int idx = millis / RS_STAT_DT_MSEC;

    if (idx < 0 || idx > RS_STAT_N)
        return 0.; // Should not happen
    return stat->data[idx - 1] * stat->norm_factor;
}

void rs_stats_init(struct rs_stats *stats) {
    rs_stat_init(&stats->tx_stat_bits, RS_STAT_AGG_SUM, "TX", "bps",
                 1000. / RS_STAT_DT_MSEC);
    rs_stat_init(&stats->tx_stat_packets, RS_STAT_AGG_COUNT, "TX", "pps",
                 1000. / RS_STAT_DT_MSEC);
    rs_stat_init(&stats->tx_stat_errors, RS_STAT_AGG_COUNT, "TX", "eps",
                 1000. / RS_STAT_DT_MSEC);

    rs_stat_init(&stats->rx_stat_bits, RS_STAT_AGG_SUM, "RX", "bps",
                 1000. / RS_STAT_DT_MSEC);
    rs_stat_init(&stats->rx_stat_packets, RS_STAT_AGG_COUNT, "RX", "pps",
                 1000. / RS_STAT_DT_MSEC);
    rs_stat_init(&stats->rx_stat_missed, RS_STAT_AGG_AVG, "RX miss", "", 1.);
    rs_stat_init(&stats->rx_stat_dt, RS_STAT_AGG_AVG, "RX dt", "ms", 1.);

    rs_stat_init(&stats->other_rx_stat_bits, RS_STAT_AGG_AVG, "-RX", "bps", 1.);
    rs_stat_init(&stats->other_rx_stat_packets, RS_STAT_AGG_AVG, "-RX", "pps",
                 1.);
    rs_stat_init(&stats->other_rx_stat_missed, RS_STAT_AGG_AVG, "-RX miss", "",
                 1.);
    rs_stat_init(&stats->other_rx_stat_dt, RS_STAT_AGG_AVG, "-RX dt", "ms", 1.);
}

void rs_stats_register_tx(struct rs_stats *stats, int bytes) {
    rs_stat_register(&stats->tx_stat_bits, 8 * bytes);
    rs_stat_register(&stats->tx_stat_packets, 1.0);
}
void rs_stats_register_rx(struct rs_stats *stats, int bytes, int missed_packets,
                          struct rs_stats_packed *received_stats,
                          uint16_t ts_sent) {

    rs_stat_register(&stats->rx_stat_bits, 8 * bytes);
    rs_stat_register(&stats->rx_stat_packets, 1.0);
    if(missed_packets < 0 || missed_packets > 1000){
        syslog(LOG_DEBUG, "Unexpected missed_packets reported");
    }else{
        for (int i = 0; i < missed_packets; i++)
            rs_stat_register(&stats->rx_stat_missed, 1.0);
    }
    rs_stat_register(&stats->rx_stat_missed, 0.0);
    rs_stat_register(&stats->rx_stat_dt, cur_msec() - ts_sent);

    rs_stat_register(&stats->other_rx_stat_bits,
                     1000 * (double)received_stats->rx_bits);
    rs_stat_register(&stats->other_rx_stat_packets,
                     (double)received_stats->rx_packets);
    rs_stat_register(&stats->other_rx_stat_missed,
                     0.0001 * (double)received_stats->rx_missed);
    rs_stat_register(&stats->other_rx_stat_dt, (double)received_stats->rx_dt);
}

void rs_stats_printf(struct rs_stats *stats) {
    rs_stat_printf(&stats->tx_stat_bits);
    rs_stat_printf(&stats->rx_stat_bits);
    printf("\n");
    rs_stat_printf(&stats->tx_stat_packets);
    rs_stat_printf(&stats->rx_stat_packets);
    rs_stat_printf(&stats->tx_stat_errors);
    rs_stat_printf(&stats->rx_stat_missed);
    printf("\n");
    rs_stat_printf(&stats->other_rx_stat_bits);
    rs_stat_printf(&stats->other_rx_stat_missed);
}

void rs_stats_packed_init(struct rs_stats_packed *packed,
                          struct rs_stats *from) {
    packed->rx_bits = rs_stat_current(&from->rx_stat_bits) / 1000;
    packed->rx_packets = rs_stat_current(&from->rx_stat_packets);
    packed->rx_missed = rs_stat_current(&from->rx_stat_missed) * 10000;
    packed->rx_dt = rs_stat_current(&from->rx_stat_dt);
}

int rs_stats_packed_pack(struct rs_stats_packed *packed, uint8_t **buffer,
                         int *buffer_len) {
    PACK(buffer, buffer_len, uint16_t, packed->rx_bits);
    PACK(buffer, buffer_len, uint16_t, packed->rx_packets);
    PACK(buffer, buffer_len, uint16_t, packed->rx_missed);
    PACK(buffer, buffer_len, uint16_t, packed->rx_dt);

    return 0;

pack_err:
    return -1;
}

int rs_stats_packed_unpack(struct rs_stats_packed *unpacked, uint8_t **buffer,
                           int *buffer_len) {
    UNPACK(buffer, buffer_len, uint16_t, &unpacked->rx_bits);
    UNPACK(buffer, buffer_len, uint16_t, &unpacked->rx_packets);
    UNPACK(buffer, buffer_len, uint16_t, &unpacked->rx_missed);
    UNPACK(buffer, buffer_len, uint16_t, &unpacked->rx_dt);

    return 0;

unpack_err:
    return -1;
}

void rs_stats_place(struct rs_stats *stats, double *into) {
    into[0] = rs_stat_current(&stats->tx_stat_bits);
    into[1] = rs_stat_current(&stats->tx_stat_packets);
    into[2] = rs_stat_current(&stats->tx_stat_errors);
    into[3] = rs_stat_current(&stats->rx_stat_bits);
    into[4] = rs_stat_current(&stats->rx_stat_packets);
    into[5] = rs_stat_current(&stats->rx_stat_missed);
    into[6] = rs_stat_current(&stats->rx_stat_dt);
    into[7] = rs_stat_current(&stats->other_rx_stat_bits);
    into[8] = rs_stat_current(&stats->other_rx_stat_packets);
    into[9] = rs_stat_current(&stats->other_rx_stat_missed);
    into[10] = rs_stat_current(&stats->other_rx_stat_dt);
}
