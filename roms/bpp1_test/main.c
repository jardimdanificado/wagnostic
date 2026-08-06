// bpp1_test - 1bpp (Monochrome) example
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
    uint8_t vram[(320 * 240) / 8];
} rom;

static int initialized = 0;
static uint32_t ticks = 0;

int wupdate() {
    ticks++;
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.a_bits = 1; rom.s.a_shift = 0;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        initialized = 1;
    }
    
    // Redraw the 40-pixel wide vertical stripes every frame to clear the old cube
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int stripe = (x / 40) % 2;
            int byte_idx = (y * 320 + x) / 8;
            int bit_idx = 7 - (x % 8);
            if (stripe) {
                rom.vram[byte_idx] |= (1 << bit_idx);
            } else {
                rom.vram[byte_idx] &= ~(1 << bit_idx);
            }
        }
    }
    
    // Draw a moving 20x20 square
    int px = (ticks / 10) % 320;
    int py = 110; 
    
    for (int sy = 0; sy < 20; sy++) {
        for (int sx = 0; sx < 20; sx++) {
            int cx = (px + sx) % 320;
            int cy = py + sy;
            int byte_idx = (cy * 320 + cx) / 8;
            int bit_idx = 7 - (cx % 8);
            rom.vram[byte_idx] ^= (1 << bit_idx); // Invert
        }
    }
    
    return (int)&rom.s;
}
