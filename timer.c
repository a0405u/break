#include "timer.h"
#include <unistd.h>

/* Convert timespec difference to seconds */
static inline double timespec_diff_sec(struct timespec a,
                                       struct timespec b)
{
    return (a.tv_sec - b.tv_sec) +
           (a.tv_nsec - b.tv_nsec) * 1e-9;
}

void timer_start(Timer *t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

double timer_elapsed(const Timer *t)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return timespec_diff_sec(now, t->start);
}

void timer_sleep(double seconds)
{
    if (seconds <= 0.0)
        return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);

    nanosleep(&ts, NULL);
}

void timer_sleep_remaining(const struct timespec *frame_start,
                           double frame_duration)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double elapsed = timespec_diff_sec(now, *frame_start);
    double remaining = frame_duration - elapsed;

    if (remaining > 0.0)
        timer_sleep(remaining);
}
