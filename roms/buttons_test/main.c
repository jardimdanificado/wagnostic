#include "piolho.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "mouse.h"

static framebuffer_t *surface;
static keyboard_t    *keyboard;
static mouse_t       *mouse;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (!surface || !surface->pixels) return;
    uint32_t* _fb = (uint32_t*)surface->pixels;
    int sw = (int)surface->width;
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= (int)surface->height) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < sw)
                _fb[iy * sw + ix] = color;
        }
    }
}

static int initialized = 0;

int32_t update(void) {
    if (!initialized) {
        surface  = (framebuffer_t*)use(FRAMEBUFFER_EXTENSION);
        keyboard = (keyboard_t*)use(KEYBOARD_EXTENSION);
        mouse    = (mouse_t*)use(MOUSE_EXTENSION);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return UPDATE_ERROR;

    uint32_t* _fb = (uint32_t*)surface->pixels;
    for (int i = 0; i < 320 * 240; i++) _fb[i] = RGB(51, 51, 51);

    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (320 - (cols * cell_w)) / 2;
    int margin_y = (240 - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        uint32_t col = RGB(119, 119, 119);
        if (keyboard && keyboard->keys[i]) col = RGB(0, 204, 85);
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;

    draw_rect(mx - 2, my - 2, 5, 5, RGB(255, 255, 255));
    if (mbtns & MOUSE_BTN_LEFT) draw_rect(mx - 4, my - 4, 9, 9, RGB(255, 0, 0));

    return UPDATE_OK;
}
