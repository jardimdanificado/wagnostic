// full_test — Comprehensive test of Piolho 2.0 features

#include "piolho.h"
#include "framebuffer.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"

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

static const uint8_t font5x7[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
};

static void draw_digit(int x, int y, int d, uint8_t r, uint8_t g, uint8_t b) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (font5x7[d][row] & (0x10 >> col))
                set_pixel(x + col, y + row, r, g, b);
}

static void draw_number(int x, int y, int n, uint8_t r, uint8_t g, uint8_t b) {
    if (n == 0) { draw_digit(x, y, 0, r, g, b); return; }
    char buf[12]; int len = 0;
    while (n > 0 && len < 12) { buf[len++] = n % 10; n /= 10; }
    for (int i = len - 1; i >= 0; i--) { draw_digit(x, y, buf[i], r, g, b); x += 6; }
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
    int mwheel = mouse ? mouse->wheel_y : 0;

    int cx = ox + (mx * qw) / (int)surface->width;
    int cy = oy + (my * qh) / (int)surface->height;

    for (int x = ox; x < ox + qw; x++) set_pixel(x, cy, 60, 60, 80);
    for (int y = oy; y < oy + qh; y++) set_pixel(cx, y, 60, 60, 80);

    fill_rect(cx - 2, cy - 2, 5, 5, 255, 255, 255);

    uint8_t lb = (mbtns & WMOUSE_BTN_LEFT) ? 255 : 80;
    fill_rect(ox + 2, oy + qh - 12, 15, 10, lb, 30, 30);

    uint8_t rb = (mbtns & WMOUSE_BTN_RIGHT) ? 100 : 80;
    fill_rect(ox + 22, oy + qh - 12, 15, 10, 30, 30, rb);

    draw_number(ox + 45, oy + qh - 12, mwheel, 255, 255, 0);
}

int32_t wupdate(void) {
    if (!initialized) {
        surface   = (wframebuffer_t*)wextension(WFRAMEBUFFER_EXTENSION);
        clock_ext = (wclock_t*)wextension(WCLOCK_EXTENSION);
        keyboard  = (wkeyboard_t*)wextension(WKEYBOARD_EXTENSION);
        mouse     = (wmouse_t*)wextension(WMOUSE_EXTENSION);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

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

    if (keyboard && keyboard->keys[41]) return WUPDATE_EXIT;

    int W = (int)surface->width, H = (int)surface->height;
    clear(15, 15, 20);

    fill_rect(W/2, 0, 1, H, 60, 60, 80);
    fill_rect(0, H/2, W, 1, 60, 60, 80);

    draw_keyboard(0, 0, W/2, H/2);
    draw_dirty_anim(W/2 + 1, 0, W/2 - 1, H/2);
    draw_mouse(0, H/2 + 1, W/2, H/2 - 1);

    return WUPDATE_OK;
}
