// input_test — Tests input extensions (keyboard, mouse, gamepad)

#include "wagnostic.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixels;
} wframebuffer_t;

typedef struct {
    uint8_t keys[256];
} wkeyboard_t;

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t buttons;
    int32_t  wheel_x;
    int32_t  wheel_y;
} wmouse_t;

#define WMOUSE_BTN_LEFT    (1 << 0)
#define WMOUSE_BTN_RIGHT   (1 << 1)

static wframebuffer_t *surface;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static void set_pixel(int x, int y, uint32_t c) {
    if (!surface || !surface->pixels) return;
    int w = (int)surface->width;
    int h = (int)surface->height;
    if (x >= 0 && x < w && y >= 0 && y < h)
        ((uint32_t*)surface->pixels)[y * w + x] = c;
}

static void fill_rect(int rx, int ry, int rw, int rh, uint32_t c) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, c);
}

static void draw_keyboard_section(void) {
    int cols = 16, rows = 16;
    int cell_w = 12, cell_h = 7;
    int ox = 8, oy = 5;

    fill_rect(ox - 2, oy - 2, cols * cell_w + 4, rows * cell_h + 4, RGB(20, 20, 30));

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = ox + cx * cell_w, py = oy + cy * cell_h;
        uint32_t col = (keyboard && keyboard->keys[i]) ? RGB(0, 220, 80) : RGB(60, 60, 70);
        fill_rect(px, py, cell_w - 1, cell_h - 1, col);
    }
}

static void draw_mouse_section(void) {
    int ox = 10, oy = 140;
    int w = 140, h = 95;

    fill_rect(ox, oy, w, h, RGB(20, 20, 30));

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    int cx = ox + 5 + (mx * (w - 10)) / 320;
    int cy = oy + 5 + (my * (h - 20)) / 240;

    fill_rect(cx - 1, cy - 1, 3, 3, RGB(255, 0, 0));

    uint32_t mbtns = mouse ? mouse->buttons : 0;
    uint32_t lc = (mbtns & WMOUSE_BTN_LEFT) ? RGB(255, 50, 50) : RGB(80, 80, 80);
    uint32_t rc = (mbtns & WMOUSE_BTN_RIGHT) ? RGB(50, 50, 255) : RGB(80, 80, 80);
    fill_rect(ox + 10, oy + h - 18, 25, 12, lc);
    fill_rect(ox + 40, oy + h - 18, 25, 12, rc);
}

static int initialized = 0;

int32_t wupdate(void) {
    if (!initialized) {
        surface  = (wframebuffer_t*)wextension("std:framebuffer");
        keyboard = (wkeyboard_t*)wextension("std:keyboard");
        mouse    = (wmouse_t*)wextension("std:mouse");

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    fill_rect(0, 0, (int)surface->width, (int)surface->height, RGB(15, 15, 20));

    draw_keyboard_section();
    draw_mouse_section();

    if (keyboard && keyboard->keys[41]) return WUPDATE_EXIT;

    return WUPDATE_OK;
}
