#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

uint32_t w_width       = 320;
uint32_t w_height      = 240;
uint32_t w_bpp         = 16;
uint32_t w_scale       = 4;
char w_title[128]      = "Buttons & Mouse Test";
uint8_t w_vram[320 * 240 * 2];
uint32_t w_dirty_count = 0;
Rect w_dirty_rects[32];
int32_t w_mouse_x      = 0;
int32_t w_mouse_y      = 0;
uint32_t w_mouse_buttons = 0;
int32_t w_mouse_wheel  = 0;
uint8_t w_keys[256]    = {0};
uint32_t w_ticks       = 0;

static uint16_t* _fb = (uint16_t*)w_vram;

static void redraw() {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, (int)w_width, (int)w_height};
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

int wupdate() {
    for (int i = 0; i < 320 * 240; i++) _fb[i] = W_RGB565(51, 51, 51);

    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (320 - (cols * cell_w)) / 2;
    int margin_y = (240 - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        uint16_t col = W_RGB565(119, 119, 119);
        if (w_keys[i]) col = W_RGB565(0, 204, 85);
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    draw_rect(w_mouse_x - 2, w_mouse_y - 2, 5, 5, W_RGB565(255, 255, 255));
    if (w_mouse_buttons & 1) draw_rect(w_mouse_x - 4, w_mouse_y - 4, 9, 9, W_RGB565(255, 0, 0));

    redraw(); return 1;
}
