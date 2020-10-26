#ifndef RS_UTIL_H
#define RS_UTIL_H

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

#endif
