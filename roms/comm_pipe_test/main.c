#include "piolho.h"
#include "comm_pipe.h"

static comm_pipe_t *pipe = 0;
static int step = 0;

int32_t update(void) {
    if (!pipe) {
        pipe = (comm_pipe_t*)use("comm:pipe");
        if (!pipe) return ERROR;
    }

    if (step == 0) {
        comm_pipe_connect(pipe, "/tmp/piolho_test.sock");
        step = 1;
        return OK;
    }

    if (step == 1) {
        if (pipe->status == COMM_STATUS_CONNECTED) {
            uint32_t msg = 0x12345678;
            tell(pipe->peer_name, &msg, sizeof(msg), 0);
            step = 2;
        } else if (pipe->status == COMM_STATUS_ERROR) {
            return DONE;
        }
        return OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(pipe->peer_name, &reply, sizeof(reply), 0);
        if (res == OK) {
            return DONE;
        }
    }

    return OK;
}
