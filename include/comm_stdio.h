/**
 * Piolho Standard Extension: comm:stdio
 * 
 * Synchronous standard I/O communication via rendezvous tell/hear:
 * - tell("stdio:out", data, size, 0)
 * - tell("stdio:err", data, size, 0)
 * - hear("stdio:in", data, size, 0)
 */

#ifndef PIOLHO_COMM_STDIO_H
#define PIOLHO_COMM_STDIO_H

#include <stdint.h>
#include "piolho.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t status;
    int32_t auto_flush;
    int32_t bytes_available;
    int32_t reserved;
} comm_stdio_t;

typedef comm_stdio_t wcomm_stdio_t;

static inline int32_t comm_stdio_print(const char *str) {
    if (!str) return 0;
    int32_t len = 0;
    while (str[len]) len++;
    return tell("stdio:out", str, len, 0);
}

static inline int32_t comm_stdio_eprint(const char *str) {
    if (!str) return 0;
    int32_t len = 0;
    while (str[len]) len++;
    return tell("stdio:err", str, len, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_STDIO_H */
