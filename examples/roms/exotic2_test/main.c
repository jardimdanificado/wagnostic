#include <stdint.h>

#define WASM_EXPORT __attribute__((visibility("default")))

typedef struct { int x, y, w, h; } Rect;

#pragma pack(push, 1)
typedef struct {
    uint32_t width, height, scale;
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
    uint32_t r_bits, r_shift, g_bits, g_shift, b_bits, b_shift, a_bits, a_shift;
    uint8_t reserved[8];
} WagnosticState;

static struct {
    WagnosticState s;
    uint8_t vram[320 * 240];
} rom;

static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t b, uint8_t a) {
    if (x < 0 || x >= (int)rom.s.width || y < 0 || y >= (int)rom.s.height) return;
    int idx = y * (int)rom.s.width + x;
    
    // R4 B3 A1
    // R in bits 4..7 (4 bits)
    // B in bits 1..3 (3 bits)
    // A in bit 0     (1 bit)
    
    uint8_t r_val = (r >> 4) & 0x0F;
    uint8_t b_val = (b >> 5) & 0x07;
    uint8_t a_val = (a >> 7) & 0x01;
    
    rom.vram[idx] = (r_val << 4) | (b_val << 1) | a_val;
}

WASM_EXPORT int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
                rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        rom.s.r_bits = 4; rom.s.r_shift = 4;
        rom.s.g_bits = 0; rom.s.g_shift = 0;
        rom.s.b_bits = 3; rom.s.b_shift = 1;
        rom.s.a_bits = 1; rom.s.a_shift = 0;
        
        char* t = rom.s.title;
        const char* src = "Exotic2 Test (R4 B3 A1)";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        
        // Draw a test pattern
        for (int y = 0; y < 240; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t r = (x * 255) / 320;
                uint8_t b = (y * 255) / 240;
                // Center circle alpha
                int dx = x - 160;
                int dy = y - 120;
                uint8_t a = (dx*dx + dy*dy < 80*80) ? 255 : 0;
                
                set_pixel(x, y, r, b, a);
            }
        }
        
        initialized = 1;
    }

    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&rom.s;
}
