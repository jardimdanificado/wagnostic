/**
 * Piolho Standard Extension: comm:broadcast
 * 
 * Multi-instance rendezvous communication via BroadcastChannel.
 */

#ifndef PIOLHO_COMM_BROADCAST_H
#define PIOLHO_COMM_BROADCAST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COMM_MODE_JOIN        0

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_ERROR     -1

typedef struct {
    char channel[64];           /* Broadcast channel name */
    int32_t mode;               /* COMM_MODE_JOIN */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;         /* Number of active peers discovered */
    char advertised_name[32];   /* Name to announce to bus */
    char peer_name[32];         /* Last discovered peer name */
    int32_t reserved;
} comm_broadcast_t;

typedef comm_broadcast_t wcomm_broadcast_t;

static inline void comm_broadcast_join(comm_broadcast_t *c, const char *channel, const char *advertised_name) {
    if (!c) return;
    int i = 0;
    while (channel && channel[i] && i < 63) { c->channel[i] = channel[i]; i++; }
    c->channel[i] = '\0';
    c->mode = COMM_MODE_JOIN;
    if (advertised_name) {
        int j = 0;
        while (advertised_name[j] && j < 31) { c->advertised_name[j] = advertised_name[j]; j++; }
        c->advertised_name[j] = '\0';
    } else {
        c->advertised_name[0] = '\0';
    }
    c->status = COMM_STATUS_CONNECTING;
}

#ifdef __cplusplus
}
#endif

#endif /* PIOLHO_COMM_BROADCAST_H */
