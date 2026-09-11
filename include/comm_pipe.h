#ifndef PIOLHO_COMM_PIPE_H
#define PIOLHO_COMM_PIPE_H

#include <stdint.h>

#define COMM_MODE_CONNECT      0  /* Active */
#define COMM_MODE_LISTEN       1  /* Passive / Discoverable */

#define COMM_STATUS_IDLE       0
#define COMM_STATUS_CONNECTING 1
#define COMM_STATUS_CONNECTED  2
#define COMM_STATUS_LISTENING  3
#define COMM_STATUS_ERROR     -1

typedef struct {
    char path[128];             /* Unix socket / named pipe path */
    int32_t mode;               /* COMM_MODE_* */
    int32_t status;             /* COMM_STATUS_* */
    int32_t peer_count;         /* Number of active peers discovered/connected */
    char advertised_name[32];   /* Name to advertise to other peers */
    char peer_name[32];         /* Last discovered/connected peer name */
} comm_pipe_t;

typedef comm_pipe_t wcomm_pipe_t;

/* Active Mode: Connect to an existing socket/pipe */
static inline void comm_pipe_connect(comm_pipe_t *c, const char *path) {
    if (!c) return;
    int i = 0;
    while (path && path[i] && i < 127) { c->path[i] = path[i]; i++; }
    c->path[i] = '\0';
    c->mode = COMM_MODE_CONNECT;
    c->status = COMM_STATUS_CONNECTING;
}

/* Passive Mode: Create socket/pipe and be discoverable by other processes */
static inline void comm_pipe_listen(comm_pipe_t *c, const char *path, const char *advertised_name) {
    if (!c) return;
    int i = 0;
    while (path && path[i] && i < 127) { c->path[i] = path[i]; i++; }
    c->path[i] = '\0';
    c->mode = COMM_MODE_LISTEN;
    if (advertised_name) {
        int j = 0;
        while (advertised_name[j] && j < 31) { c->advertised_name[j] = advertised_name[j]; j++; }
        c->advertised_name[j] = '\0';
    } else {
        c->advertised_name[0] = '\0';
    }
    c->status = COMM_STATUS_CONNECTING;
}

#endif /* PIOLHO_COMM_PIPE_H */
