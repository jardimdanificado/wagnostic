typedef struct { int x, y, w, h; } Rect;
// bpp1_test - 1bpp (Monochrome) example
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





typedef struct {
    uint32_t width, height, scale;
    char title[128];
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t audio_chunk_samples, audio_volume, audio_paused;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint8_t reserved[504];
} State;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[(320 * 240) / 8];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        
        rom.s.a_bits = 1; rom.s.a_shift = 0;

        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        char* t = rom.s.title;
        const char* src = "1bpp Test (Monochrome)";
        int i = 0; while (src[i] && i < 127) { t[i] = src[i]; i++; } t[i] = '\0';
        
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
    int px = (rom.s.ticks / 10) % 320;
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
    
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
