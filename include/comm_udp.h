#ifndef PIOLHO_COMM_UDP_H
#define PIOLHO_COMM_UDP_H

#include <stdint.h>

#define COMM_MODE_PROBE        0  /* Active: Probe remote node */
#define COMM_MODE_LISTEN       1  /* Passive: Listen and respond to probes/discovery */
#define COMM_MODE_BEACON       2  /* Active Beacon: Broadcast discoverability */

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_LISTENING  3
#define COMM_STATUS_ERROR     -1

typedef struct {
    char host[64];              /* Target host / broadcast address */
    int32_t port;               /* Port number */
    int32_t mode;               /* COMM_MODE_* */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;         /* Number of active peers discovered/connected */
    char advertised_name[32];   /* Name to advertise to other peers */
    char peer_name[32];         /* Last discovered/connected peer name */
} comm_udp_t;

typedef comm_udp_t wcomm_udp_t;

/* Active Mode: Probe/Ping a specific host/port or subnet */
static inline void comm_udp_probe(comm_udp_t *c, const char *host, int32_t port) {
    if (!c) return;
    int i = 0;
    while (host && host[i] && i < 63) { c->host[i] = host[i]; i++; }
    c->host[i] = '\0';
    c->port = port;
    c->mode = COMM_MODE_PROBE;
    c->status = COMM_STATUS_CONNECTING;
}

/* Passive Mode: Listen on port, making this instance discoverable to incoming probes */
static inline void comm_udp_listen(comm_udp_t *c, int32_t port, const char *advertised_name) {
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

/* Active Beacon: Broadcast presence on network */
static inline void comm_udp_beacon(comm_udp_t *c, const char *broadcast_ip, int32_t port, const char *advertised_name) {
    if (!c) return;
    int i = 0;
    while (broadcast_ip && broadcast_ip[i] && i < 63) { c->host[i] = broadcast_ip[i]; i++; }
    c->host[i] = '\0';
    c->port = port;
    c->mode = COMM_MODE_BEACON;
    if (advertised_name) {
        int j = 0;
        while (advertised_name[j] && j < 31) { c->advertised_name[j] = advertised_name[j]; j++; }
        c->advertised_name[j] = '\0';
    } else {
        c->advertised_name[0] = '\0';
    }
    c->status = COMM_STATUS_CONNECTING;
}

#endif /* PIOLHO_COMM_UDP_H */
