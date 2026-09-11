/**
 * Piolho Standard Extension: comm:webtransport
 * 
 * WebTransport (HTTP/3 QUIC) communication.
 */

#ifndef PIOLHO_COMM_WEBTRANSPORT_H
#define PIOLHO_COMM_WEBTRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_ERROR     -1

typedef struct {
    char url[128];
    int32_t mode;
    int32_t status;
    int32_t peer_count;
    char advertised_name[32];
    char peer_name[32];
} comm_webtransport_t;

typedef comm_webtransport_t wcomm_webtransport_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_WEBTRANSPORT_H */
