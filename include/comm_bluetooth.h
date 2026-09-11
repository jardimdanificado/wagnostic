/**
 * Piolho Standard Extension: comm:bluetooth
 * 
 * Bluetooth Low Energy communication (Web Bluetooth API).
 */

#ifndef PIOLHO_COMM_BLUETOOTH_H
#define PIOLHO_COMM_BLUETOOTH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_ERROR     -1

typedef struct {
    char service_uuid[64];
    char char_uuid[64];
    int32_t status;
    int32_t peer_count;
    char advertised_name[32];
    char peer_name[32];
} comm_bluetooth_t;

typedef comm_bluetooth_t wcomm_bluetooth_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_BLUETOOTH_H */
