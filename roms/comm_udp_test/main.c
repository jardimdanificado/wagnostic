#include "piolho.h"
#include "comm_udp.h"

static comm_udp_t *udp = 0;
static int step = 0;

int32_t update(void) {
    if (!udp) {
        udp = (comm_udp_t*)use("comm:udp");
        if (!udp) return ERROR;
    }

    if (step == 0) {
        comm_udp_probe(udp, "127.0.0.1", 9999);
        step = 1;
        return OK;
    }

    if (step == 1) {
        if (udp->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0x99887766;
            tell(udp->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (udp->status == COMM_STATUS_ERROR) {
            return DONE;
        }
        return OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(udp->peer_name, &reply, sizeof(reply), 0);
        if (res == OK) {
            return DONE;
        }
    }

    return OK;
}
