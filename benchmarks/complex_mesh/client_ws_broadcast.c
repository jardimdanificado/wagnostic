#include "piolho.h"
#include "comm_ws.h"
#include "comm_broadcast.h"
#include "comm_stdio.h"
#include "comm_shm.h"
#include "comm_workers.h"

#define PACKET_MAGIC 0xCAFEBABE

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t link_type;
    uint32_t csum;
    uint8_t  payload[32];
} mesh_packet_t;

static comm_ws_t        *ws_ext = 0;
static comm_broadcast_t *bc_ext = 0;

static int step = 0;
static uint32_t seq_counter = 0;

int32_t update(void) {
    if (step == 0) {
        ws_ext = (comm_ws_t*)ask("comm:ws");
        bc_ext = (comm_broadcast_t*)ask("comm:broadcast");
        ask("comm:stdio");
        ask("comm:shm");
        ask("comm:workers");

        if (ws_ext) comm_ws_connect(ws_ext, "ws://127.0.0.1:9302");
        if (bc_ext) comm_broadcast_join(bc_ext, "mesh_bus", "gamma_bc");

        step = 1;
        return OK;
    }

    // Pump WebSocket
    if (ws_ext && ws_ext->status == COMM_STATUS_CONNECTED && ws_ext->peer_name[0]) {
        mesh_packet_t pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 4 };
        tell(ws_ext->peer_name, &pkt, sizeof(pkt), 0);
    }

    // Pump BroadcastChannel
    if (bc_ext && bc_ext->status == COMM_STATUS_CONNECTED && bc_ext->peer_name[0]) {
        mesh_packet_t pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 6 };
        tell(bc_ext->peer_name, &pkt, sizeof(pkt), 0);
    }

    // Pump Local Memory IPC
    mesh_packet_t local_pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 1 };
    tell("gamma_worker2", &local_pkt, sizeof(local_pkt), 1);

    // Consume replies
    mesh_packet_t reply;
    while (hear(ANY, &reply, sizeof(reply), 0) == OK) {}

    step++;
    return OK;
}
