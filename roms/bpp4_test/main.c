typedef struct { int x, y, w, h; } Rect;
// bpp4_test - 4bpp (16 colors) example
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
    int32_t unique;
    uint8_t reserved[500];
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

        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        char* t = rom.s.title;
        const char* src = "4bpp Test (16 Colors EGA)";
        int i = 0; while (src[i] && i < 127) { t[i] = src[i]; i++; } t[i] = '\0';
        

        
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
