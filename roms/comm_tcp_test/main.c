#include "piolho.h"
#include "comm_tcp.h"

static comm_tcp_t *tcp = 0;
static int step = 0;

int32_t update(void) {
    if (!tcp) {
        tcp = (comm_tcp_t*)ask("comm:tcp");
        if (!tcp) return ERROR;
    }

    if (step == 0) {
        comm_tcp_connect(tcp, "127.0.0.1", 9871);
        step = 1;
        return OK;
    }

    if (step == 1) {
        if (tcp->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0xCAFEBABE;
            tell(tcp->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (tcp->status == COMM_STATUS_ERROR) {
            return DONE;
        }
        return OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(tcp->peer_name, &reply, sizeof(reply), 0);
        if (res == OK && reply == 0xDEADBEEF) {
            return DONE;
        }
    }

    return OK;
}
