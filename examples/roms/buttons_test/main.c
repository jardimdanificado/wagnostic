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
    uint32_t io_load, io_load_buffer, io_load_size;
    uint32_t io_save, io_save_buffer, io_save_size;
    uint8_t reserved[16];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

static uint16_t* _fb = (uint16_t*)rom.vram;

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static void redraw() {
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, (int)rom.s.width, (int)rom.s.height};
}
static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= 240) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < 320)
                _fb[iy * 320 + ix] = color;
        }
    }
}

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.scale = 4;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        char* t = rom.s.title;
        const char* src = "Buttons & Mouse Test";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        initialized = 1;
    }

    for (int i = 0; i < 320 * 240; i++) _fb[i] = W_RGB565(51, 51, 51);

    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (320 - (cols * cell_w)) / 2;
    int margin_y = (240 - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        uint16_t col = W_RGB565(119, 119, 119);
        if (rom.s.keys[i]) col = W_RGB565(0, 204, 85);
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    draw_rect(rom.s.mouse_x - 2, rom.s.mouse_y - 2, 5, 5, W_RGB565(255, 255, 255));
    if (rom.s.mouse_buttons & 1) draw_rect(rom.s.mouse_x - 4, rom.s.mouse_y - 4, 9, 9, W_RGB565(255, 0, 0));

    redraw();
    return (int)&rom.s;
}
