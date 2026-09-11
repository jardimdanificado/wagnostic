/**
 * Piolho Standard Extension: comm:workers
 * 
 * Capability indicator for worker threads / sub-instances.
 * Calling use("comm:workers") returns 1 if supported, 0 otherwise.
 */

#ifndef PIOLHO_COMM_WORKERS_H
#define PIOLHO_COMM_WORKERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_WORKERS_AVAILABLE 1
#define COMM_WORKERS_UNAVAILABLE 0

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_WORKERS_H */
