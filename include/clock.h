#ifndef PIOLHO_CLOCK_H
#define PIOLHO_CLOCK_H

#include <stdint.h>

#define WCLOCK_EXTENSION "std:clock"

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    float    delta;
} wclock_t;

#endif /* PIOLHO_CLOCK_H */
