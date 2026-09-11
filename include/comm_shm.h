/**
 * Piolho Standard Extension: comm:shm
 * 
 * SharedArrayBuffer & Atomics capability indicator.
 */

#ifndef PIOLHO_COMM_SHM_H
#define PIOLHO_COMM_SHM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t status;
    int32_t shm_size;
    int32_t peer_count;
    int32_t reserved;
} comm_shm_t;

typedef comm_shm_t wcomm_shm_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_SHM_H */
