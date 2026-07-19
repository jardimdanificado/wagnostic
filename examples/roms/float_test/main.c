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
    uint8_t vram[320 * 240 * (128 / 8)];
} rom;

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
                rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        
rom.s.r_bits = 32; rom.s.r_shift = 0;
rom.s.g_bits = 32; rom.s.g_shift = 32;
rom.s.b_bits = 32; rom.s.b_shift = 64;
rom.s.a_bits = 32; rom.s.a_shift = 96;
rom.s.is_float = 1;

        
        const char* title = "RGBA32323232F Example";
        for (int i = 0; i < 127 && title[i]; i++) rom.s.title[i] = title[i];
        
        initialized = 1;
    }
    
    float* fb = (float*)rom.vram;
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int idx = (y * 320 + x) * (128 / 8 / sizeof(float));
            
            fb[idx + 0] = (float)x / 320.0f;
            fb[idx + 1] = (float)y / 240.0f;
            fb[idx + 2] = 0.5f;
            fb[idx + 3] = 1.0f;

        }
    }
    
    
    
    rom.s.dirty_rects = 0;
    return (int)&rom.s;
}
