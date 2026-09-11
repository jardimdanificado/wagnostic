#ifndef PIOLHO_CLOCK_H
#define PIOLHO_CLOCK_H

#include <stdint.h>

#define CLOCK_EXTENSION  "std:clock"
#define WCLOCK_EXTENSION "std:clock"

typedef struct {
    uint64_t ticks;      /* Total monotonic ticks */
    uint64_t frequency;  /* Ticks per second */
    float    delta;      /* Elapsed seconds since last frame */
} clock_ext_t;

#define delta_time delta

typedef clock_ext_t pclock_t;
typedef clock_ext_t wclock_t;

#endif /* PIOLHO_CLOCK_H */
