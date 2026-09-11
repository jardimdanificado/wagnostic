#ifndef PIOLHO_COMM_TCP_H
#define PIOLHO_COMM_TCP_H

#include <stdint.h>

#define COMM_MODE_CONNECT      0  /* Active: Initiate connection to remote peer */
#define COMM_MODE_LISTEN       1  /* Passive: Listen & be discoverable by remote peers */

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_LISTENING  3
#define COMM_STATUS_ERROR     -1

typedef struct {
    char host[64];              /* Target host to connect or bind interface */
    int32_t port;               /* Port number */
    int32_t mode;               /* COMM_MODE_* */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;         /* Number of active peers discovered/connected */
    char advertised_name[32];   /* Name to advertise to other peers (optional) */
    char peer_name[32];         /* Last discovered/connected peer name */
} comm_tcp_t;

typedef comm_tcp_t wcomm_tcp_t;

/* Active Mode: Connect to a specific remote node */
static inline void comm_tcp_connect(comm_tcp_t *c, const char *host, int32_t port) {
    if (!c) return;
    int i = 0;
    while (host && host[i] && i < 63) { c->host[i] = host[i]; i++; }
    c->host[i] = '\0';
    c->port = port;
    c->mode = COMM_MODE_CONNECT;
    c->status = COMM_STATUS_CONNECTING;
}

/* Passive Mode: Listen and make this instance discoverable by other nodes */
static inline void comm_tcp_listen(comm_tcp_t *c, int32_t port, const char *advertised_name) {
    if (!c) return;
    c->host[0] = '\0';
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

#endif /* PIOLHO_COMM_TCP_H */
