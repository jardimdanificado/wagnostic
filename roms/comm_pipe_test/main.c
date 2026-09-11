#include "piolho.h"
#include "comm_pipe.h"

static comm_pipe_t *pipe;
static int step = 0;

int32_t setup(void) {
    pipe = (comm_pipe_t*)use("comm:pipe");
    if (!pipe) return -1;
    return 0;
}

int32_t update(void) {
    if (!pipe) return UPDATE_ERROR;

    if (step == 0) {
        /* Discover and connect to local Unix domain socket / pipe instance */
        comm_pipe_connect(pipe, "/tmp/piolho_pipe_test.sock");
        step = 1;
        return UPDATE_OK;
    }

    if (step == 1) {
        if (pipe->status == COMM_STATUS_CONNECTED) {
            uint32_t data = 0x12345678;
            tell(pipe->peer_name, &data, sizeof(data), 0);
            step = 2;
        } else if (pipe->status == COMM_STATUS_ERROR) {
            return UPDATE_EXIT;
        }
        return UPDATE_OK;
    }

    if (step == 2) {
        uint32_t reply = 0;
        int32_t res = hear(pipe->peer_name, &reply, sizeof(reply), 0);
        if (res == IPC_OK) {
            return UPDATE_EXIT;
        }
    }

    return UPDATE_OK;
}
