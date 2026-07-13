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
    uint8_t reserved[40];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 1];
} rom;

static void redraw() {
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, (int)rom.s.width, (int)rom.s.height};
}

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 8;
        rom.s.scale = 1;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        char* t = rom.s.title;
        const char* src = "Fallback Test";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        initialized = 1;
    }

    uint8_t* fb = (uint8_t*)rom.vram;
    static uint32_t last_tick = 0;
    static uint8_t color = 0;

    uint32_t now = rom.s.ticks;
    if (now - last_tick > 1000) {
        color += 32;
        last_tick = now;
    }

    for (int i = 0; i < 320 * 240; i++)
        fb[i] = color;

    redraw();
    return (int)&rom.s;
}
