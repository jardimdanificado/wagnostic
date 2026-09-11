#include "piolho.h"
#include "comm_ws.h"

static comm_ws_t *ws = 0;
static int step = 0;

int32_t update(void) {
    if (!ws) {
        ws = (comm_ws_t*)ask("comm:ws");
        if (!ws) return ERROR;
    }

    if (step == 0) {
        comm_ws_connect(ws, "ws://127.0.0.1:8765");
        step = 1;
        return OK;
    }

    if (step == 1) {
        if (ws->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0x55AA55AA;
            tell(ws->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (ws->status == COMM_STATUS_ERROR) {
            return DONE;
        }
        return OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(ws->peer_name, &reply, sizeof(reply), 0);
        if (res == OK) {
            return DONE;
        }
    }

    return OK;
}
