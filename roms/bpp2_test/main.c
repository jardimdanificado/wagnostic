typedef struct { int x, y, w, h; } Rect;
// bpp2_test - 2bpp (4 colors) example
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
    uint8_t vram[(320 * 240) / 4];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.a_bits = 2; rom.s.a_shift = 0;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        // Fill VRAM with 4 large colored blocks
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                int color_idx = 0;
                if (x >= 160 && y < 120) color_idx = 1;
                else if (x < 160 && y >= 120) color_idx = 2;
                else if (x >= 160 && y >= 120) color_idx = 3;
                
                int byte_idx = (y * 320 + x) / 4;
                int shift = 6 - ((x % 4) * 2);
                
                // Clear bits
                rom.vram[byte_idx] &= ~(3 << shift);
                // Set bits
                rom.vram[byte_idx] |= (color_idx << shift);
            }
        }
        initialized = 1;
    }
    
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
