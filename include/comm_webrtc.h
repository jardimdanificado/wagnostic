/**
 * Piolho Standard Extension: comm:webrtc
 * 
 * WebRTC DataChannel communication capability and endpoint.
 */

#ifndef PIOLHO_COMM_WEBRTC_H
#define PIOLHO_COMM_WEBRTC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_ERROR     -1

typedef struct {
    char signaling_url[64];
    int32_t mode;
    int32_t status;
    int32_t peer_count;
    char advertised_name[32];
    char peer_name[32];
    int32_t reserved;
} comm_webrtc_t;

typedef comm_webrtc_t wcomm_webrtc_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_WEBRTC_H */
