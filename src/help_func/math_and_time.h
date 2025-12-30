#pragma once

#include <time.h>

struct timespec ifb_mat_timespec_ms(time_t ms) {
    struct timespec ts = {0};
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    return ts;
}
