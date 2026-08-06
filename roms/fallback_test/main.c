#include <stdint.h>

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 1];
} rom;

static int initialized = 0;
static uint32_t ticks = 0;

int wupdate() {
    ticks++;
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.r_bits = 3; rom.s.r_shift = 5;
        rom.s.g_bits = 3; rom.s.g_shift = 2;
        rom.s.b_bits = 2; rom.s.b_shift = 0;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);

        initialized = 1;
    }

    uint8_t* fb = (uint8_t*)rom.vram;
    static uint32_t last_tick = 0;
    static uint8_t color = 0;

    if (ticks - last_tick > 60) {
        color += 32;
        last_tick = ticks;
    }

    for (int i = 0; i < 320 * 240; i++)
        fb[i] = color;

    return (int)&rom.s;
}
