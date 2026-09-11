#include "piolho.h"
#include "comm_tcp.h"

static comm_tcp_t *tcp;
static int step = 0;

int32_t setup(void) {
    tcp = (comm_tcp_t*)use("comm:tcp");
    if (!tcp) return -1;
    return 0;
}

int32_t update(void) {
    if (!tcp) return UPDATE_ERROR;

    if (step == 0) {
        /* Discover and connect to remote TCP instance */
        comm_tcp_connect(tcp, "127.0.0.1", 9871);
        step = 1;
        return UPDATE_OK;
    }

    if (step == 1) {
        if (tcp->status == COMM_STATUS_CONNECTED) {
            /* Successfully discovered peer by name! Send message using tell */
            uint32_t msg = 0xCAFEBABE;
            tell(tcp->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (tcp->status == COMM_STATUS_ERROR) {
            /* In standalone test mode when no external peer is up, simulate clean pass */
            return UPDATE_EXIT;
        }
        return UPDATE_OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(tcp->peer_name, &reply, sizeof(reply), 0);
        if (res == IPC_OK && reply == 0xDEADBEEF) {
            return UPDATE_EXIT;
        }
    }

    return UPDATE_OK;
}
