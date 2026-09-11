#include "piolho.h"
#include "comm_udp.h"

static comm_udp_t *udp;
static int step = 0;

int32_t setup(void) {
    udp = (comm_udp_t*)use("comm:udp");
    if (!udp) return -1;
    return 0;
}

int32_t update(void) {
    if (!udp) return UPDATE_ERROR;

    if (step == 0) {
        /* Probe/ping remote UDP endpoint to discover peer name */
        comm_udp_probe(udp, "127.0.0.1", 9999);
        step = 1;
        return UPDATE_OK;
    }

    if (step == 1) {
        if (udp->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0x99887766;
            tell(udp->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (udp->status == COMM_STATUS_ERROR) {
            return UPDATE_EXIT;
        }
        return UPDATE_OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(udp->peer_name, &reply, sizeof(reply), 0);
        if (res == IPC_OK) {
            return UPDATE_EXIT;
        }
    }

    return UPDATE_OK;
}
