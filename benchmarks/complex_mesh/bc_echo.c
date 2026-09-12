#include "piolho.h"
#include "comm_broadcast.h"

static comm_broadcast_t *bc_ext = 0;
static int step = 0;

int32_t update(void) {
    if (step == 0) {
        bc_ext = (comm_broadcast_t*)ask("comm:broadcast");
        if (bc_ext) {
            comm_broadcast_join(bc_ext, "mesh_bus", "bc_peer");
        }
        step = 1;
        return OK;
    }

    uint8_t buf[128];
    while (hear(ANY, buf, sizeof(buf), 0) == OK) {
        if (bc_ext && bc_ext->status == COMM_STATUS_CONNECTED && bc_ext->peer_name[0]) {
            tell(bc_ext->peer_name, buf, sizeof(buf), 0);
        }
    }

    return OK;
}
