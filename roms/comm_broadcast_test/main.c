#include "piolho.h"
#include "comm_broadcast.h"

static comm_broadcast_t *bc = 0;
static int step = 0;

int32_t update(void) {
    if (!bc) {
        bc = (comm_broadcast_t*)ask("comm:broadcast");
        if (!bc) return ERROR;
    }

    if (step == 0) {
        comm_broadcast_join(bc, "test_bus", "node_test");
        step = 1;
        return OK;
    }

    return DONE;
}
