#ifndef PIOLHO_CLOCK_H
#define PIOLHO_CLOCK_H

#include <stdint.h>

#define CLOCK_EXTENSION "clock"

typedef struct {
    uint64_t ticks;      /* Total monotonic ticks */
    uint64_t frequency;  /* Ticks per second */
    float    delta;      /* Elapsed seconds since last step */
} clock_ext_t;

typedef clock_ext_t pclock_t;

#endif /* PIOLHO_CLOCK_H */
