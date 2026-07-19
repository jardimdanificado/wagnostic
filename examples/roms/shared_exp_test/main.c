#include <stdint.h>

typedef struct {
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    struct { int x, y, w, h; } dirty_rects[32];
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
    uint8_t r_bits;
    uint8_t r_shift;
    uint8_t g_bits;
    uint8_t g_shift;
    uint8_t b_bits;
    uint8_t b_shift;
    uint8_t a_bits;
    uint8_t a_shift;
    uint8_t is_signed;
    uint8_t is_float;
    uint8_t is_shared_exponent;
    uint8_t reserved[29];
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
        rom.s.bpp = 32;
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        
        
rom.s.r_bits = 9; rom.s.r_shift = 0;
rom.s.g_bits = 9; rom.s.g_shift = 9;
rom.s.b_bits = 9; rom.s.b_shift = 18;
rom.s.a_bits = 5; rom.s.a_shift = 27; // The shared exponent
rom.s.is_shared_exponent = 1;

        
        const char* title = "RGB9E5 Example";
        for (int i = 0; i < 127 && title[i]; i++) rom.s.title[i] = title[i];
        
        initialized = 1;
    }
    
    // Draws a gradient using a shared 5-bit exponent to control brightness
    uint32_t* fb = (uint32_t*)rom.vram;
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int idx = (y * 320 + x) * (32 / 8 / sizeof(uint32_t));
            
            // Beautiful smooth gradient using RGB9E5
            // 9-bit mantissas give us 512 levels of precision per color.
            uint32_t r = (x * 511) / 320; // 0 to 511
            uint32_t g = (y * 511) / 240; // 0 to 511
            uint32_t b = 511;
            
            // Constant exponent of 15 (bias) means multiplier is 2^(15-15) = 1.0
            uint32_t exp = 15;
            
            fb[idx] = r | (g << 9) | (b << 18) | (exp << 27);

        }
    }
    
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0].x = 0;
    rom.s.dirty_rects[0].y = 0;
    rom.s.dirty_rects[0].w = 320;
    rom.s.dirty_rects[0].h = 240;
    
    return (int)&rom.s;
}
