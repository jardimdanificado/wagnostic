#include <stdint.h>

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

static struct {
    State s;
    uint8_t vram[320 * 240 * (32 / 8)];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
                rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        
rom.s.r_bits = 8; rom.s.r_shift = 0;
rom.s.g_bits = 8; rom.s.g_shift = 8;
rom.s.b_bits = 8; rom.s.b_shift = 16;
rom.s.a_bits = 8; rom.s.a_shift = 24;
rom.s.is_signed = 1;

        
        const char* title = "RGBA8SNORM Example";
        for (int i = 0; i < 127 && title[i]; i++) rom.s.title[i] = title[i];
        
        initialized = 1;
    }
    
    int8_t* fb = (int8_t*)rom.vram;
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int idx = (y * 320 + x) * (32 / 8 / sizeof(int8_t));
            
            // Let's make R vary from 0 to 127 (0 to 255 mapped)
            // Let's make B vary from -128 to 127 (mapped to 0 up to 255)
            fb[idx + 0] = (int8_t)((x * 127) / 320); // R: 0 to 127
            fb[idx + 1] = 0;                         // G: 0
            fb[idx + 2] = (int8_t)(((y * 255) / 240) - 128); // B: -128 to 127
            fb[idx + 3] = 127;                       // A: max

        }
    }
    
    
    
    rom.s.dirty_rects = 0;
    return (int)&rom.s;
}
