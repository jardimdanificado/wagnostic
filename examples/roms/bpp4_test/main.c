// bpp4_test - 4bpp (16 colors) example
#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    Rect dirty_rects[32];
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
    uint32_t r_bits;
    uint32_t r_shift;
    uint32_t g_bits;
    uint32_t g_shift;
    uint32_t b_bits;
    uint32_t b_shift;
    uint32_t a_bits;
    uint32_t a_shift;
    uint8_t reserved[8];
} State;

static struct {
    State s;
    uint8_t vram[(320 * 240) / 2];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 4;
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
    
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&rom.s;
}
