#include "piolho.h"
#include "comm_tcp.h"
#include "comm_pipe.h"
#include "comm_ws.h"
#include "comm_udp.h"
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

static comm_tcp_t       *tcp_ext  = 0;
static comm_pipe_t      *pipe_ext = 0;
static comm_ws_t        *ws_ext   = 0;
static comm_udp_t       *udp_ext  = 0;
static comm_broadcast_t *bc_ext   = 0;
static comm_stdio_t     *stdio_ext = 0;
static comm_shm_t       *shm_ext  = 0;

static int step = 0;
static uint32_t seq_counter = 0;

int32_t update(void) {
    if (step == 0) {
        tcp_ext   = (comm_tcp_t*)ask("comm:tcp");
        pipe_ext  = (comm_pipe_t*)ask("comm:pipe");
        ws_ext    = (comm_ws_t*)ask("comm:ws");
        udp_ext   = (comm_udp_t*)ask("comm:udp");
        bc_ext    = (comm_broadcast_t*)ask("comm:broadcast");
        stdio_ext = (comm_stdio_t*)ask("comm:stdio");
        shm_ext   = (comm_shm_t*)ask("comm:shm");
        int32_t has_workers = (int32_t)(uintptr_t)ask("comm:workers");

        if (tcp_ext)  comm_tcp_listen(tcp_ext, 9301, "hub_tcp");
        if (pipe_ext) comm_pipe_listen(pipe_ext, "/tmp/piolho_mesh.sock", "hub_pipe");
        if (ws_ext)   comm_ws_listen(ws_ext, 9302, "hub_ws");
        if (udp_ext)  comm_udp_listen(udp_ext, 9303, "hub_udp");
        if (bc_ext)   comm_broadcast_join(bc_ext, "mesh_bus", "hub_bc");

        step = 1;
        return OK;
    }

    // Process incoming
    mesh_packet_t in_pkt;
    while (hear(ANY, &in_pkt, sizeof(in_pkt), 0) == OK) {}

    // Local Memory IPC ping-pong with sibling worker
    mesh_packet_t local_pkt = {
        .magic = PACKET_MAGIC,
        .seq = ++seq_counter,
        .link_type = 1
    };
    tell("hub_worker2", &local_pkt, sizeof(local_pkt), 1);

    // Stdio pulse
    if (step % 100 == 0) {
        tell("stdio:out", "[Hub Tick]\n", 11, 0);
    }
    step++;

    return OK;
}
