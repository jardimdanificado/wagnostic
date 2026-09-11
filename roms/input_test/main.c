// input_test — Tests all input methods

#include "wagnostic.h"
#include "framebuffer.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"

static wframebuffer_t *surface;
static wclock_t       *clock_ext;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;
static wgamepad_t     *gamepad;

static uint32_t ticks = 0;
static int initialized = 0;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static void set_pixel(int x, int y, uint32_t c) {
    if (!surface || !surface->pixels) return;
    int w = (int)surface->width;
    int h = (int)surface->height;
    if (x >= 0 && x < w && y >= 0 && y < h) {
        uint32_t *fb = (uint32_t*)surface->pixels;
        fb[y * w + x] = c;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint32_t c) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, c);
}

static void draw_hline(int x1, int x2, int y, uint32_t c) {
    for (int x = x1; x < x2; x++) set_pixel(x, y, c);
}

static void draw_vline(int x, int y1, int y2, uint32_t c) {
    for (int y = y1; y < y2; y++) set_pixel(x, y, c);
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

static void draw_digit(int x, int y, int d, uint32_t c) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (font5x7[d][row] & (0x10 >> col))
                set_pixel(x + col, y + row, c);
}

static void draw_number(int x, int y, int n, uint32_t c) {
    if (n == 0) { draw_digit(x, y, 0, c); return; }
    char buf[12]; int len = 0;
    int tmp = n;
    if (tmp < 0) { set_pixel(x, y, c); tmp = -tmp; x += 7; }
    while (tmp > 0 && len < 12) { buf[len++] = tmp % 10; tmp /= 10; }
    for (int i = len - 1; i >= 0; i--) {
        draw_digit(x, y, buf[i], c);
        x += 6;
    }
}

static void draw_keyboard_section(void) {
    int cols = 16, rows = 16;
    int cell_w = 12, cell_h = 7;
    int ox = 8, oy = 5;

    fill_rect(ox - 2, oy - 2, cols * cell_w + 4, rows * cell_h + 4, RGB(20, 20, 30));

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = ox + cx * cell_w, py = oy + cy * cell_h;
        int is_pressed = keyboard && keyboard->keys[i];
        uint32_t col = is_pressed ? RGB(0, 220, 80) : RGB(60, 60, 70);
        fill_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    draw_number(ox, oy + rows * cell_h + 4, (int)ticks / 60, RGB(200, 200, 200));
}

static void draw_mouse_section(void) {
    int ox = 10, oy = 140;
    int w = 140, h = 95;

    fill_rect(ox, oy, w, h, RGB(20, 20, 30));
    draw_hline(ox, ox + w, oy, RGB(80, 80, 80));
    draw_vline(ox, oy, oy + h, RGB(80, 80, 80));

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;
    int mwheel = mouse ? mouse->wheel_y : 0;

    int cx = ox + 5 + (mx * (w - 10)) / 320;
    int cy = oy + 5 + (my * (h - 20)) / 240;
    draw_hline(cx - 8, cx + 8, cy, RGB(255, 255, 255));
    draw_vline(cx, cy - 8, cy + 8, RGB(255, 255, 255));
    fill_rect(cx - 1, cy - 1, 3, 3, RGB(255, 0, 0));

    uint32_t lc = (mbtns & WMOUSE_BTN_LEFT) ? RGB(255, 50, 50) : RGB(80, 80, 80);
    uint32_t rc = (mbtns & WMOUSE_BTN_RIGHT) ? RGB(50, 50, 255) : RGB(80, 80, 80);
    fill_rect(ox + 10, oy + h - 18, 25, 12, lc);
    fill_rect(ox + 40, oy + h - 18, 25, 12, rc);

    draw_number(ox + 80, oy + h - 18, mwheel, RGB(255, 255, 0));
}

static void draw_gamepad_section(void) {
    int ox = 170, oy = 140;
    int w = 145, h = 95;

    fill_rect(ox, oy, w, h, RGB(20, 20, 30));
    draw_hline(ox, ox + w, oy, RGB(80, 80, 80));

    uint32_t gp = gamepad ? gamepad->buttons : 0;

    int bx = ox + 10, by = oy + 10;
    uint32_t dc = RGB(100, 100, 100);
    fill_rect(bx + 10, by, 10, 10, (gp & WGAMEPAD_BTN_DPAD_UP) ? RGB(0,255,0) : dc);
    fill_rect(bx + 10, by + 22, 10, 10, (gp & WGAMEPAD_BTN_DPAD_DOWN) ? RGB(0,255,0) : dc);
    fill_rect(bx, by + 11, 10, 10, (gp & WGAMEPAD_BTN_DPAD_LEFT) ? RGB(0,255,0) : dc);
    fill_rect(bx + 20, by + 11, 10, 10, (gp & WGAMEPAD_BTN_DPAD_RIGHT) ? RGB(0,255,0) : dc);
    fill_rect(bx + 10, by + 11, 10, 10, RGB(50,50,50));

    fill_rect(bx + 45, by + 5, 15, 15, (gp & WGAMEPAD_BTN_A) ? RGB(255,50,50) : dc);
    fill_rect(bx + 65, by + 5, 15, 15, (gp & WGAMEPAD_BTN_B) ? RGB(50,50,255) : dc);
    fill_rect(bx + 45, by + 25, 15, 10, (gp & WGAMEPAD_BTN_SELECT) ? RGB(200,200,0) : dc);
    fill_rect(bx + 65, by + 25, 15, 10, (gp & WGAMEPAD_BTN_START) ? RGB(200,200,0) : dc);

    draw_number(bx, by + 50, (int)gp, RGB(180, 180, 180));
}

int32_t wupdate(void) {
    ticks++;
    if (!initialized) {
        surface   = (wframebuffer_t*)wextension(WFRAMEBUFFER_EXTENSION);
        clock_ext = (wclock_t*)wextension(WCLOCK_EXTENSION);
        keyboard  = (wkeyboard_t*)wextension(WKEYBOARD_EXTENSION);
        mouse     = (wmouse_t*)wextension(WMOUSE_EXTENSION);
        gamepad   = (wgamepad_t*)wextension(WGAMEPAD_EXTENSION);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint32_t* fb = (uint32_t*)surface->pixels;
    for (int i = 0; i < 320 * 240; i++) fb[i] = RGB(15, 15, 20);

    draw_keyboard_section();
    draw_mouse_section();
    draw_gamepad_section();

    if (keyboard && keyboard->keys[41]) return WUPDATE_EXIT; // Escape

    return WUPDATE_OK;
}
