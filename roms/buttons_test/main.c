typedef struct { int x, y, w, h; } Rect;
#include <stdint.h>
#include <stddef.h>

extern void* wextension(const char* name, void* ptr);

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
    uint32_t dirty_rects;
} State;

typedef struct {
    int32_t x, y;
    uint32_t buttons;
    int32_t wheel;
} MouseState;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

static uint16_t* _fb = (uint16_t*)rom.vram;

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static void redraw() {
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, (int)rom.s.width, (int)rom.s.height};
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
static uint8_t* keys = NULL;
static MouseState* mouse = NULL;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 6; rom.s.g_shift = 5;
        rom.s.b_bits = 5; rom.s.b_shift = 0;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);

        keys = (uint8_t*)wextension("std:keyboard", NULL);
        mouse = (MouseState*)wextension("std:mouse", NULL);

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
        if (keys && keys[i]) col = W_RGB565(0, 204, 85);
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;

    draw_rect(mx - 2, my - 2, 5, 5, W_RGB565(255, 255, 255));
    if (mbtns & 1) draw_rect(mx - 4, my - 4, 9, 9, W_RGB565(255, 0, 0));

    redraw();
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
