/**
 * Piolho Standard Extension: comm:serial
 * 
 * Hardware Serial Port communication (Web Serial / serialport).
 */

#ifndef PIOLHO_COMM_SERIAL_H
#define PIOLHO_COMM_SERIAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_ERROR     -1

typedef struct {
    char device[64];            /* Device path or identifier */
    int32_t baud_rate;          /* Baud rate (e.g. 115200, 9600) */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;
    char advertised_name[32];
    char peer_name[32];
} comm_serial_t;

typedef comm_serial_t wcomm_serial_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_SERIAL_H */
