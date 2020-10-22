#ifndef RS_UTIL_H
#define RS_UTIL_H

#include <time.h>

#define rs_offset_of(_struct_, _member_)                                       \
    (size_t) & (((struct _struct_ *)0)->_member_)

#define rs_cast(_subclass_, _superclass_pt_)                                   \
                                                                               \
    ((struct _subclass_ *)((unsigned char *)(_superclass_pt_)-rs_offset_of(    \
        _subclass_, super)))

inline long msec_diff(struct timespec t1, struct timespec t2){
    return (t1.tv_sec - t2.tv_sec) * 1000L + (t1.tv_nsec - t2.tv_nsec) / 1000000L;
}

#endif
