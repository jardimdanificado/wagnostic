/**
 * Piolho Standard Extension: comm:http
 * 
 * HTTP streaming / fetch communication endpoint.
 */

#ifndef PIOLHO_COMM_HTTP_H
#define PIOLHO_COMM_HTTP_H

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
    int32_t method;             /* 0 = GET, 1 = POST */
    int32_t status;             /* COMM_STATUS_* */
    int32_t response_code;      /* HTTP response status code */
    int32_t peer_count;
    char advertised_name[32];
    char peer_name[32];
} comm_http_t;

typedef comm_http_t wcomm_http_t;

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_HTTP_H */
