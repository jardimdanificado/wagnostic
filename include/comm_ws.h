#ifndef PIOLHO_COMM_WS_H
#define PIOLHO_COMM_WS_H

#include <stdint.h>

#define COMM_MODE_CONNECT      0  /* Active client */
#define COMM_MODE_LISTEN       1  /* Passive server / endpoint */

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_LISTENING  3
#define COMM_STATUS_ERROR     -1

typedef struct {
    char url[128];              /* WebSocket URL (client) */
    int32_t port;               /* Listen port (server) */
    int32_t mode;               /* COMM_MODE_* */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;         /* Number of active peers discovered/connected */
    char advertised_name[32];   /* Name to advertise to other peers */
    char peer_name[32];         /* Last discovered/connected peer name */
} comm_ws_t;

typedef comm_ws_t wcomm_ws_t;

/* Active Mode: Connect to remote WebSocket endpoint */
static inline void comm_ws_connect(comm_ws_t *c, const char *url) {
    if (!c) return;
    int i = 0;
    while (url && url[i] && i < 127) { c->url[i] = url[i]; i++; }
    c->url[i] = '\0';
    c->mode = COMM_MODE_CONNECT;
    c->status = COMM_STATUS_CONNECTING;
}

/* Passive Mode: Start WebSocket server and be discoverable */
static inline void comm_ws_listen(comm_ws_t *c, int32_t port, const char *advertised_name) {
    if (!c) return;
    c->port = port;
    c->mode = COMM_MODE_LISTEN;
    if (advertised_name) {
        int i = 0;
        while (advertised_name[i] && i < 31) { c->advertised_name[i] = advertised_name[i]; i++; }
        c->advertised_name[i] = '\0';
    } else {
        c->advertised_name[0] = '\0';
    }
    c->status = COMM_STATUS_CONNECTING;
}

#endif /* PIOLHO_COMM_WS_H */
