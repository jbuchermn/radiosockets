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

    // [...last_data...]|[data / n_data...000]
    //                  t0
    double last_data[RS_STAT_N];
    struct timespec t0;
    double data[RS_STAT_N];
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

#endif
