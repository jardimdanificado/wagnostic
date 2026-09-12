// full_test — Comprehensive test of Wagnostic 2.0 features

#include "wagnostic.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixels;
} wframebuffer_t;

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    float    delta;
} wclock_t;

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

#define WMOUSE_BTN_LEFT  (1 << 0)
#define WMOUSE_BTN_RIGHT (1 << 1)

static wframebuffer_t *surface;
static wclock_t       *clock_ext;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static int frame_count = 0;
static int resize_state = 0;
static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!surface || !surface->pixels) return;
    int w = (int)surface->width;
    int h = (int)surface->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    int idx = y * w + x;
    ((uint32_t*)surface->pixels)[idx] = RGB(r, g, b);
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    if (!surface) return;
    fill_rect(0, 0, (int)surface->width, (int)surface->height, r, g, b);
}

static void draw_keyboard(int ox, int oy, int qw, int qh) {
    int cols = 8, rows = 8;
    int cell_w = qw / (cols + 1), cell_h = qh / (rows + 2);
    int start_key = (frame_count / 120) % 3;

    for (int i = 0; i < 64; i++) {
        int key_idx = start_key * 64 + i;
        if (key_idx >= 256) break;
        int cx = i % cols, cy = i / cols;
        int px = ox + 4 + cx * cell_w, py = oy + 12 + cy * cell_h;
        int is_pressed = keyboard && keyboard->keys[key_idx];
        uint8_t cr = is_pressed ? 0 : 50;
        uint8_t cg = is_pressed ? 200 : 50;
        uint8_t cb = is_pressed ? 80 : 60;
        fill_rect(px, py, cell_w - 1, cell_h - 1, cr, cg, cb);
    }
}

static int anim_x = 0, anim_y = 0, anim_dx = 2, anim_dy = 1;

static void draw_dirty_anim(int ox, int oy, int qw, int qh) {
    anim_x += anim_dx; anim_y += anim_dy;
    if (anim_x <= 0 || anim_x + 15 >= qw) anim_dx = -anim_dx;
    if (anim_y <= 0 || anim_y + 15 >= qh) anim_dy = -anim_dy;

    for (int y = 0; y < qh; y += 8)
        for (int x = 0; x < qw; x += 8)
            fill_rect(ox + x, oy + y, 7, 7,
                      ((x / 8 + y / 8) % 2) ? 40 : 25,
                      ((x / 8 + y / 8) % 2) ? 40 : 25,
                      ((x / 8 + y / 8) % 2) ? 50 : 35);

    fill_rect(ox + anim_x, oy + anim_y, 15, 15, 255, 200, 0);
    fill_rect(ox + anim_x - anim_dx, oy + anim_y - anim_dy, 5, 5, 100, 80, 0);
}

static void draw_mouse(int ox, int oy, int qw, int qh) {
    if (!surface) return;
    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;

    int cx = ox + (mx * qw) / (int)surface->width;
    int cy = oy + (my * qh) / (int)surface->height;

    for (int x = ox; x < ox + qw; x++) set_pixel(x, cy, 60, 60, 80);
    for (int y = oy; y < oy + qh; y++) set_pixel(cx, y, 60, 60, 80);

    fill_rect(cx - 2, cy - 2, 5, 5, 255, 255, 255);

    uint8_t lb = (mbtns & WMOUSE_BTN_LEFT) ? 255 : 80;
    fill_rect(ox + 2, oy + qh - 12, 15, 10, lb, 30, 30);

    uint8_t rb = (mbtns & WMOUSE_BTN_RIGHT) ? 100 : 80;
    fill_rect(ox + 22, oy + qh - 12, 15, 10, 30, 30, rb);
}

int32_t update(void) {
    if (!initialized) {
        surface   = (wframebuffer_t*)ask("std:framebuffer");
        clock_ext = (wclock_t*)ask("std:clock");
        keyboard  = (wkeyboard_t*)ask("std:keyboard");
        mouse     = (wmouse_t*)ask("std:mouse");

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return UPDATE_ERROR;

    frame_count++;

    static int r_was = 0;

    int key_r = keyboard ? keyboard->keys[21] : 0;
    if (key_r && !r_was) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) { surface->width = 320; surface->height = 240; }
        else if (resize_state == 1) { surface->width = 640; surface->height = 480; }
        else { surface->width = 160; surface->height = 120; }
    }
    r_was = key_r;

    if (keyboard && keyboard->keys[41]) return UPDATE_EXIT;

    int W = (int)surface->width, H = (int)surface->height;
    clear(15, 15, 20);

    fill_rect(W/2, 0, 1, H, 60, 60, 80);
    fill_rect(0, H/2, W, 1, 60, 60, 80);

    draw_keyboard(0, 0, W/2, H/2);
    draw_dirty_anim(W/2 + 1, 0, W/2 - 1, H/2);
    draw_mouse(0, H/2 + 1, W/2, H/2 - 1);

    return UPDATE_OK;
}
