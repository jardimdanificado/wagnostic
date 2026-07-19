// bpp2_test - 2bpp (4 colors) example
#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

static struct {
    uint32_t count;
    Rect rects[32];
} my_dirty_list;

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
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint32_t is_signed, is_float, is_shared_exponent;
    uint8_t reserved[504];
} State;

static void SET_BPP(State* s, int bpp) {
    if (bpp == 32) {
        s->a_bits = 8; s->a_shift = 24; s->b_bits = 8; s->b_shift = 16;
        s->g_bits = 8; s->g_shift = 8;  s->r_bits = 8; s->r_shift = 0;
    } else if (bpp == 24) {
        s->a_bits = 0; s->a_shift = 0;  s->b_bits = 8; s->b_shift = 16;
        s->g_bits = 8; s->g_shift = 8;  s->r_bits = 8; s->r_shift = 0;
    } else if (bpp == 16) {
        s->a_bits = 0; s->a_shift = 0;  s->r_bits = 5; s->r_shift = 11;
        s->g_bits = 6; s->g_shift = 5;  s->b_bits = 5; s->b_shift = 0;
    } else if (bpp == 8) {
        s->a_bits = 0; s->a_shift = 0;  s->r_bits = 3; s->r_shift = 5;
        s->g_bits = 3; s->g_shift = 2;  s->b_bits = 2; s->b_shift = 0;
    } else if (bpp == 4 || bpp == 2 || bpp == 1) {
        s->a_bits = bpp; s->a_shift = 0;
        s->r_bits = 0; s->r_shift = 0;
        s->g_bits = 0; s->g_shift = 0;
        s->b_bits = 0; s->b_shift = 0;
    }
}

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

        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        char* t = rom.s.title;
        const char* src = "2bpp Test (GameBoy Style)";
        int i = 0; while (src[i] && i < 127) { t[i] = src[i]; i++; } t[i] = '\0';
        
        // GameBoy Palette
        
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
        SET_BPP(&rom.s, 2);
    }
    
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
