#include "piolho.h"
#include "comm_tcp.h"
#include "comm_pipe.h"
#include "comm_udp.h"
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

static comm_tcp_t  *tcp_ext  = 0;
static comm_pipe_t *pipe_ext = 0;
static comm_udp_t  *udp_ext  = 0;
static comm_stdio_t *stdio_ext = 0;

static int step = 0;
static uint32_t seq_counter = 0;

int32_t update(void) {
    if (step == 0) {
        tcp_ext   = (comm_tcp_t*)ask("comm:tcp");
        pipe_ext  = (comm_pipe_t*)ask("comm:pipe");
        udp_ext   = (comm_udp_t*)ask("comm:udp");
        stdio_ext = (comm_stdio_t*)ask("comm:stdio");
        ask("comm:shm");
        ask("comm:workers");

        if (tcp_ext)  comm_tcp_connect(tcp_ext, "127.0.0.1", 9301);
        if (pipe_ext) comm_pipe_connect(pipe_ext, "/tmp/piolho_mesh.sock");
        if (udp_ext)  comm_udp_probe(udp_ext, "127.0.0.1", 9303);

        step = 1;
        return OK;
    }

    // Pump TCP stream
    if (tcp_ext && tcp_ext->status == COMM_STATUS_CONNECTED && tcp_ext->peer_name[0]) {
        mesh_packet_t pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 2 };
        tell(tcp_ext->peer_name, &pkt, sizeof(pkt), 0);
    }

    // Pump Pipe stream
    if (pipe_ext && pipe_ext->status == COMM_STATUS_CONNECTED && pipe_ext->peer_name[0]) {
        mesh_packet_t pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 3 };
        tell(pipe_ext->peer_name, &pkt, sizeof(pkt), 0);
    }

    // Pump UDP datagram
    if (udp_ext && udp_ext->status == COMM_STATUS_CONNECTED && udp_ext->peer_name[0]) {
        mesh_packet_t pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 5 };
        tell(udp_ext->peer_name, &pkt, sizeof(pkt), 0);
    }

    // Pump Local Memory IPC
    mesh_packet_t local_pkt = { .magic = PACKET_MAGIC, .seq = ++seq_counter, .link_type = 1 };
    tell("beta_worker2", &local_pkt, sizeof(local_pkt), 1);

    // Consume any replies
    mesh_packet_t reply;
    while (hear(ANY, &reply, sizeof(reply), 0) == OK) {}

    step++;
    return OK;
}
