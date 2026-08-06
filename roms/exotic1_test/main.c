typedef struct { int x, y, w, h; } Rect;
#include <stdint.h>

/* SET_BPP(s, bpp) — sets channel bits/shifts for standard pixel formats.
 * The host derives BPP from these fields; there is no separate bpp field. */
#define SET_BPP(s, bpp_val) do { \
    if ((bpp_val) == 32) { \
        (s)->r_bits=8;(s)->r_shift=0; \
        (s)->g_bits=8;(s)->g_shift=8; \
        (s)->b_bits=8;(s)->b_shift=16; \
        (s)->a_bits=8;(s)->a_shift=24; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } else if ((bpp_val) == 16) { \
        (s)->r_bits=5;(s)->r_shift=11; \
        (s)->g_bits=6;(s)->g_shift=5; \
        (s)->b_bits=5;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } else if ((bpp_val) == 8) { \
        (s)->r_bits=3;(s)->r_shift=5; \
        (s)->g_bits=3;(s)->g_shift=2; \
        (s)->b_bits=2;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } \
} while(0)



#define WASM_EXPORT __attribute__((visibility("default")))



typedef struct {
    uint32_t width, height, scale;
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[676];
} WagnosticState;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    WagnosticState s;
    uint16_t vram[320 * 240];
} rom;

static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= (int)rom.s.width || y < 0 || y >= (int)rom.s.height) return;
    int idx = y * (int)rom.s.width + x;
    
    // R5 G1 B5 A5
    // R in bits 11..15 (5 bits)
    // G in bit 10      (1 bit)
    // B in bits 5..9   (5 bits)
    // A in bits 0..4   (5 bits)
    
    uint16_t r_val = (r >> 3) & 0x1F;
    uint16_t g_val = (g >> 7) & 0x01;
    uint16_t b_val = (b >> 3) & 0x1F;
    uint16_t a_val = (a >> 3) & 0x1F;
    
    rom.vram[idx] = (r_val << 11) | (g_val << 10) | (b_val << 5) | a_val;
}

WASM_EXPORT int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 1; rom.s.g_shift = 10;
        rom.s.b_bits = 5; rom.s.b_shift = 5;
        rom.s.a_bits = 5; rom.s.a_shift = 0;
        

        
        // Draw a test pattern
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t r = (x * 255) / 320;
                uint8_t b = (y * 255) / 240;
                // Since G only has 1 bit, we will make it alternate columns to be visible
                uint8_t g = (x / 20) % 2 == 0 ? 255 : 0;
                // Alpha fade out to the right
                uint8_t a = 255 - ((x * 255) / 320);
                
                set_pixel(x, y, r, g, b, a);
            }
        }
        
        initialized = 1;
    }

    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
