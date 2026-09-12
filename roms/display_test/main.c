// display_test — Tests surface modes and formats

#include "wagnostic.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixels;
} wframebuffer_t;

typedef struct {
    uint8_t keys[256];
} wkeyboard_t;

static wframebuffer_t *surface;
static wkeyboard_t    *keyboard;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static int frame_phase = 0;
static int resize_state = 0;
static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!surface || !surface->pixels) return;
    int w = (int)surface->width;
    int h = (int)surface->height;
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    int idx = y * w + x;
    uint32_t* fb = (uint32_t*)surface->pixels;
    fb[idx] = RGB(r, g, b);
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear_screen(uint8_t r, uint8_t g, uint8_t b) {
    if (!surface) return;
    fill_rect(0, 0, (int)surface->width, (int)surface->height, r, g, b);
}

static void draw_color_bars(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    int bar_w = w / 8;
    uint8_t colors[8][3] = {
        {255,255,255}, {255,255,0}, {0,255,255}, {0,255,0},
        {255,0,255}, {255,0,0}, {0,0,255}, {0,0,0}
    };
    for (int i = 0; i < 8; i++) {
        fill_rect(i * bar_w, 0, bar_w, h - 30,
                  colors[i][0], colors[i][1], colors[i][2]);
    }
}

static void draw_grid(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    for (int x = 0; x < w; x += 32) {
        for (int y = 0; y < h; y++)
            set_pixel(x, y, 128, 128, 128);
    }
    for (int y = 0; y < h; y += 32) {
        for (int x = 0; x < w; x++)
            set_pixel(x, y, 128, 128, 128);
    }
}

static void draw_status(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    fill_rect(0, h - 30, w, 30, 0, 0, 0);
    fill_rect(5, h - 25, 20, 20, 0, 200, 255);
    fill_rect(35, h - 25, 10, 10, 255, 255, 255);
    fill_rect(50, h - 25, 10, 10, 200, 200, 200);
}

int32_t update(void) {
    if (!initialized) {
        surface  = (wframebuffer_t*)ask("std:framebuffer");
        keyboard = (wkeyboard_t*)ask("std:keyboard");

        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }

        initialized = 1;
    }

    if (!surface) return UPDATE_ERROR;

    frame_phase++;

    static int r_was_down = 0;

    int r_down = keyboard ? keyboard->keys[21] : 0;
    if (r_down && !r_was_down) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) {
            surface->width = 320; surface->height = 240;
        } else if (resize_state == 1) {
            surface->width = 640; surface->height = 480;
        } else {
            surface->width = 160; surface->height = 120;
        }
    }
    r_was_down = r_down;

    clear_screen(32, 32, 32);
    draw_color_bars();
    draw_grid();
    draw_status();

    int ax = (frame_phase * 3) % (int)surface->width;
    fill_rect(ax, 10, 20, 20, 255, 200, 0);

    return UPDATE_OK;
}
