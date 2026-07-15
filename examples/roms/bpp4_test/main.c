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
    uint32_t palette_offset;
    uint32_t palette_count;
    uint8_t reserved[32];
} State;

static struct {
    State s;
    uint32_t palette[16];
    uint8_t vram[(320 * 240) / 2];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 4;
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        rom.s.palette_offset = (uint32_t)((uint8_t*)rom.palette - (uint8_t*)&rom.s);
        rom.s.palette_count = 16;
        
        char* t = rom.s.title;
        const char* src = "4bpp Test (16 Colors EGA)";
        int i = 0; while (src[i] && i < 127) { t[i] = src[i]; i++; } t[i] = '\0';
        
        // Classic 16 color palette
        uint32_t ega[16] = {
            0xFF000000, 0xFFAA0000, 0xFF00AA00, 0xFFAAAA00,
            0xFF0000AA, 0xFFAA00AA, 0xFF00AAAA, 0xFFAAAAAA,
            0xFF555555, 0xFFFF5555, 0xFF55FF55, 0xFFFFFF55,
            0xFF5555FF, 0xFFFF55FF, 0xFF55FFFF, 0xFFFFFFFF
        };
        for (int j = 0; j < 16; j++) rom.palette[j] = ega[j];
        
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
