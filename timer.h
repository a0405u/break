#ifndef TIMER_H
#define TIMER_H

#include <time.h>

/* Simple monotonic timer */
typedef struct {
    struct timespec start;
} Timer;

/* Initialize / reset timer */
void timer_start(Timer *t);

/* Seconds elapsed since timer_start() */
double timer_elapsed(const Timer *t);

/* Sleep for a given number of seconds (sub-second supported) */
void timer_sleep(double seconds);

/* Sleep until a fixed frame duration has passed */
void timer_sleep_remaining(const struct timespec *frame_start,
                           double frame_duration);

#endif /* TIMER_H */
