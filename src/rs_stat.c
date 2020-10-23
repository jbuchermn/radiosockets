#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "rs_stat.h"
#include "rs_util.h"

void rs_stat_init(struct rs_stat *stat, int aggregate, const char *title,
                  const char *unit) {
    clock_gettime(CLOCK_REALTIME, &stat->t0);
    stat->t0.tv_nsec %= 1000000L;
    stat->aggregate = aggregate;
    stat->title = title;
    stat->unit = unit;

    memset(stat->last_data, 0, RS_STAT_N * sizeof(int));
    memset(stat->data, 0, RS_STAT_N * sizeof(int));
    memset(stat->n_data, 0, RS_STAT_N * sizeof(int));
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
            stat->data[idx] = (stat->n_data[idx] / (stat->n_data[idx] + 1)) *
                                  stat->data[idx] +
                              value / (stat->n_data[idx] + 1);
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
    memset(stat->last_data, 0, RS_STAT_N * sizeof(double));
    for (int i = idx - RS_STAT_N, j = 0; i < RS_STAT_N; i++, j++) {
        stat->last_data[j] = stat->data[i];
    }
    memset(stat->data, 0, RS_STAT_N * sizeof(double));
    memset(stat->n_data, 0, RS_STAT_N * sizeof(int));

    long t0_msec = stat->t0.tv_sec * 1000L + stat->t0.tv_nsec / 1000000L;
    t0_msec += idx * RS_STAT_DT_MSEC;

    stat->t0.tv_sec = t0_msec / 1000L;
    stat->t0.tv_nsec = (t0_msec % 1000L) * 1000000L;
}

void rs_stat_printf(struct rs_stat *stat) {
    rs_stat_flush(stat);

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long millis = msec_diff(now, stat->t0);
    int idx = millis / RS_STAT_DT_MSEC;

    printf("STAT[%10s]: ", stat->title);
    for (int i = 0; i < RS_STAT_N - idx; i++) {
        if(stat->last_data[i] == 0.0) printf("             ");
        else printf("%+.2e%3s ", stat->last_data[i], stat->unit);
    }
    for (int i = 0; i < idx; i++) {
        if(stat->data[i] == 0.0) printf("             ");
        else printf("%+.2e%3s ", stat->data[i], stat->unit);
    }

    printf("\n");
}