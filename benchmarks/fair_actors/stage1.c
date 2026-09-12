#include "piolho.h"

typedef struct {
    uint32_t seq;
    uint32_t hash;
    uint8_t  data[128];
} packet_t;

static uint32_t seq = 0;

static uint32_t compute_work(uint32_t seed) {
    uint32_t h = seed;
    for (int i = 0; i < 30000; i++) {
        h = (h ^ (h << 13)) + 0x5bd1e995 + i;
        h = (h ^ (h >> 17)) * 0x27d4eb2d;
    }
    return h;
}

int32_t update(void) {
    packet_t pkt;
    pkt.seq = ++seq;
    pkt.hash = compute_work(pkt.seq);
    for (int i = 0; i < 128; i++) pkt.data[i] = (uint8_t)(pkt.hash + i);

    tell("stage2", &pkt, sizeof(pkt), 5);
    return OK;
}
