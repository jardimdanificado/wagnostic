typedef struct { int x, y, w, h; } Rect;
// bpp4_test - 4bpp (16 colors) example
#include <stdint.h>

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
    uint32_t dirty_rects;
} State;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[(320 * 240) / 2];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.a_bits = 4; rom.s.a_shift = 0;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        // Fill VRAM with color bars
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x += 2) {
                uint8_t color1 = (x / 20) % 16;
                uint8_t color2 = ((x + 1) / 20) % 16;
                rom.vram[(y * 320 + x) / 2] = (color1 << 4) | color2;
            }
        }
        initialized = 1;
    }
    
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
