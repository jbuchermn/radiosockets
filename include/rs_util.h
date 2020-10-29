#ifndef RS_UTIL_H
#define RS_UTIL_H

// #define __NO_TIMER__

#include <time.h>
#include <stdint.h>

#define rs_offset_of(_struct_, _member_)                                       \
    (size_t) & (((struct _struct_ *)0)->_member_)

#define rs_cast(_subclass_, _superclass_pt_)                                   \
                                                                               \
    ((struct _subclass_ *)((unsigned char *)(_superclass_pt_)-rs_offset_of(    \
        _subclass_, super)))

inline long msec_diff(struct timespec t1, struct timespec t2){
    return (t1.tv_sec - t2.tv_sec) * 1000L + (t1.tv_nsec - t2.tv_nsec) / 1000000L;
}

/* Two LSB of timestamp in millis */
inline uint16_t cur_msec(){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (ts.tv_nsec / 1000000L) + (ts.tv_sec * 1000L);
}

inline struct timespec rs_timespec_plus_ms(struct timespec* in, double ms){
    struct timespec res;
    uint64_t nsec = in->tv_nsec + 1000000000L * in->tv_sec;
    nsec += 1000000L * ms;
    res.tv_nsec = nsec % 1000000000L;
    res.tv_sec = nsec / 1000000000L;
    return res;
}


#define PACK(buffer, buffer_len, type, val) \
    if (*buffer_len < sizeof(type)) \
        goto pack_err; \
    for (int PACK_i = sizeof(type) - 1; PACK_i >= 0; PACK_i--) { \
        (**buffer) = (uint8_t)((val) >> (8 * PACK_i)); \
        (*buffer)++; \
        (*buffer_len)--; \
    }

#define UNPACK(buffer, buffer_len, type, pval) \
    if (*buffer_len < sizeof(type)) \
        goto unpack_err; \
    *pval = 0; \
    for (int UNPACK_i = sizeof(type) - 1; UNPACK_i >= 0; UNPACK_i--) { \
        *(pval) += ((type)(**buffer)) << (8 * UNPACK_i); \
        (*buffer)++; \
        (*buffer_len)--; \
    }

#ifdef __NO_TIMER__

#define TIMER_START(TNAME) ;
#define TIMER_STOP(TNAME, TBYTES) ;
#define TIMER_PRINT(TNAME, TFREQ) ;

#else

#define TIMER_START(TNAME) \
    static struct timespec TIMER_ ## TNAME ## _start; \
    static struct timespec TIMER_ ## TNAME ## _end; \
    static struct timespec TIMER_ ## TNAME ## _last_print = { 0 }; \
    static struct timespec TIMER_ ## TNAME ## _print; \
    static int TIMER_ ## TNAME ## _n_agg = 0; \
    static long long int TIMER_ ## TNAME ## _nsec_agg = 0; \
    static int TIMER_ ## TNAME ## _bytes_through_agg = 0; \
    clock_gettime(CLOCK_REALTIME, &TIMER_ ## TNAME ## _start);

#define TIMER_STOP(TNAME, TBYTES) \
    clock_gettime(CLOCK_REALTIME, &TIMER_ ## TNAME ## _end); \
    TIMER_ ## TNAME ## _n_agg++; \
    TIMER_ ## TNAME ## _bytes_through_agg+=TBYTES; \
    TIMER_ ## TNAME ## _nsec_agg += TIMER_ ## TNAME ## _end.tv_nsec - TIMER_ ## TNAME ## _start.tv_nsec; \
    TIMER_ ## TNAME ## _nsec_agg += 1000000000 * (TIMER_ ## TNAME ## _end.tv_sec - TIMER_ ## TNAME ## _start.tv_sec);

#define TIMER_PRINT(TNAME, TFREQ) \
    clock_gettime(CLOCK_REALTIME, &TIMER_ ## TNAME ## _print); \
    if(msec_diff(TIMER_ ## TNAME ## _print, TIMER_ ## TNAME ## _last_print) > 1000. / TFREQ){ \
        syslog(LOG_DEBUG, "TIMER[%s]: %fms, %5.2fMbps\n", #TNAME, \
                (double)TIMER_ ## TNAME ## _nsec_agg / TIMER_ ## TNAME ## _n_agg / 1000000., \
                1000000000. * 8 * (double)TIMER_ ## TNAME ## _bytes_through_agg / TIMER_ ## TNAME ## _nsec_agg); \
        TIMER_ ## TNAME ## _n_agg = 0; \
        TIMER_ ## TNAME ## _bytes_through_agg = 0; \
        TIMER_ ## TNAME ## _nsec_agg = 0; \
        TIMER_ ## TNAME ## _last_print = TIMER_ ## TNAME ## _print; \
    }

#endif

#endif
