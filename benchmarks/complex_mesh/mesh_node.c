/**
 * Universal Mesh Node ROM
 * 
 * Capable of initiating, echoing, and cross-routing packets across:
 * - Local Memory IPC
 * - TCP (comm:tcp)
 * - Unix Pipe (comm:pipe)
 * - WebSocket (comm:ws)
 * - UDP (comm:udp)
 * - BroadcastChannel (comm:broadcast)
 * - Standard I/O (comm:stdio)
 */

#include "piolho.h"
#include "comm_tcp.h"
#include "comm_pipe.h"
#include "comm_ws.h"
#include "comm_udp.h"
#include "comm_broadcast.h"
#include "comm_stdio.h"

#define PACKET_MAGIC 0xCAFEBABE

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t link_type; // 1=LOCAL, 2=TCP, 3=PIPE, 4=WS, 5=UDP, 6=BROADCAST
    uint32_t checksum;
    uint32_t counter;
    uint8_t  data[28];
} mesh_packet_t;

static comm_tcp_t       *tcp_ext  = 0;
static comm_pipe_t      *pipe_ext = 0;
static comm_ws_t        *ws_ext   = 0;
static comm_udp_t       *udp_ext  = 0;
static comm_broadcast_t *bc_ext   = 0;
static comm_stdio_t     *stdio_ext = 0;

static uint32_t seq_out = 0;
static uint32_t packets_received = 0;
static uint32_t packets_sent = 0;
static int initialized = 0;

static void init_node(void) {
    tcp_ext   = (comm_tcp_t*)ask("comm:tcp");
    pipe_ext  = (comm_pipe_t*)ask("comm:pipe");
    ws_ext    = (comm_ws_t*)ask("comm:ws");
    udp_ext   = (comm_udp_t*)ask("comm:udp");
    bc_ext    = (comm_broadcast_t*)ask("comm:broadcast");
    stdio_ext = (comm_stdio_t*)ask("comm:stdio");
    initialized = 1;
}

static inline uint32_t calc_checksum(uint32_t magic, uint32_t seq, uint32_t link_type, uint32_t counter) {
    return magic ^ (seq * 31) ^ (link_type * 17) ^ counter;
}

static void send_mesh_packet(const char *target, uint32_t link_type) {
    if (!target || target[0] == '\0') return;
    mesh_packet_t pkt;
    pkt.magic = PACKET_MAGIC;
    pkt.seq = ++seq_out;
    pkt.link_type = link_type;
    pkt.counter = packets_sent;
    pkt.checksum = calc_checksum(pkt.magic, pkt.seq, pkt.link_type, pkt.counter);
    for (int i = 0; i < 28; i++) pkt.data[i] = (uint8_t)(i + (seq_out & 0xFF));

    int32_t res = tell(target, &pkt, sizeof(pkt), 0);
    if (res == OK) {
        packets_sent++;
    }
}

int32_t update(void) {
    if (!initialized) {
        init_node();
    }

    // 1. Process any incoming packets from any sender (Local or Remote)
    mesh_packet_t in_pkt;
    int32_t res = hear(ANY, &in_pkt, sizeof(in_pkt), 0);
    while (res == OK) {
        if (in_pkt.magic == PACKET_MAGIC) {
            uint32_t expected_csum = calc_checksum(in_pkt.magic, in_pkt.seq, in_pkt.link_type, in_pkt.counter);
            if (in_pkt.checksum == expected_csum) {
                packets_received++;
            }
        }
        res = hear(ANY, &in_pkt, sizeof(in_pkt), 0);
    }

    // 2. Active connections: pump outbound data across active peers
    if (tcp_ext && tcp_ext->status == COMM_STATUS_CONNECTED && tcp_ext->peer_name[0] != '\0') {
        send_mesh_packet(tcp_ext->peer_name, 2);
    }

    if (pipe_ext && pipe_ext->status == COMM_STATUS_CONNECTED && pipe_ext->peer_name[0] != '\0') {
        send_mesh_packet(pipe_ext->peer_name, 3);
    }

    if (ws_ext && ws_ext->status == COMM_STATUS_CONNECTED && ws_ext->peer_name[0] != '\0') {
        send_mesh_packet(ws_ext->peer_name, 4);
    }

    if (bc_ext && bc_ext->status == COMM_STATUS_CONNECTED && bc_ext->peer_name[0] != '\0') {
        send_mesh_packet(bc_ext->peer_name, 6);
    }

    return OK;
}
