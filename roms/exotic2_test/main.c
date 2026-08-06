#include <stdint.h>

#define WASM_EXPORT __attribute__((visibility("default")))

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
} WagnosticState;

static struct {
    WagnosticState s;
    uint8_t vram[320 * 240];
} rom;

static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t b, uint8_t a) {
    if (x < 0 || x >= (int)rom.s.width || y < 0 || y >= (int)rom.s.height) return;
    int idx = y * (int)rom.s.width + x;
    
    uint8_t r_val = (r >> 4) & 0x0F;
    uint8_t b_val = (b >> 5) & 0x07;
    uint8_t a_val = (a >> 7) & 0x01;
    
    rom.vram[idx] = (r_val << 4) | (b_val << 1) | a_val;
}

WASM_EXPORT int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        rom.s.r_bits = 4; rom.s.r_shift = 4;
        rom.s.g_bits = 0; rom.s.g_shift = 0;
        rom.s.b_bits = 3; rom.s.b_shift = 1;
        rom.s.a_bits = 1; rom.s.a_shift = 0;
        
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t r = (x * 255) / 320;
                uint8_t b = (y * 255) / 240;
                int dx = x - 160;
                int dy = y - 120;
                uint8_t a = (dx*dx + dy*dy < 80*80) ? 255 : 0;
                
                set_pixel(x, y, r, b, a);
            }
        }
        
        initialized = 1;
    }

    return (int)&rom.s;
}
