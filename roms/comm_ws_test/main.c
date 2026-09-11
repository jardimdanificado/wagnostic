#include "piolho.h"
#include "comm_ws.h"

static comm_ws_t *ws;
static int step = 0;

int32_t setup(void) {
    ws = (comm_ws_t*)use("comm:ws");
    if (!ws) return -1;
    return 0;
}

int32_t update(void) {
    if (!ws) return UPDATE_ERROR;

    if (step == 0) {
        /* Discover and connect to WebSocket endpoint */
        comm_ws_connect(ws, "ws://127.0.0.1:8765");
        step = 1;
        return UPDATE_OK;
    }

    if (step == 1) {
        if (ws->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0x55AA55AA;
            tell(ws->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (ws->status == COMM_STATUS_ERROR) {
            return UPDATE_EXIT;
        }
        return UPDATE_OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(ws->peer_name, &reply, sizeof(reply), 0);
        if (res == IPC_OK) {
            return UPDATE_EXIT;
        }
    }

    return UPDATE_OK;
}
