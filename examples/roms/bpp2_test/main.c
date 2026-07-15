// bpp2_test - 2bpp (4 colors) example
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
    uint32_t palette[4];
    uint8_t vram[(320 * 240) / 4];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 2;
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        rom.s.palette_offset = (uint32_t)((uint8_t*)rom.palette - (uint8_t*)&rom.s);
        rom.s.palette_count = 4;
        
        char* t = rom.s.title;
        const char* src = "2bpp Test (GameBoy Style)";
        int i = 0; while (src[i] && i < 127) { t[i] = src[i]; i++; } t[i] = '\0';
        
        // GameBoy Palette
        rom.palette[0] = 0xFF2B3315;
        rom.palette[1] = 0xFF7B993D;
        rom.palette[2] = 0xFFA9CC66;
        rom.palette[3] = 0xFFD7FF99;
        
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
    
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&rom.s;
}
