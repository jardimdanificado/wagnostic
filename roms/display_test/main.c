// display_test — Tests all video modes

#include <stdint.h>
#include <stddef.h>

extern void* wextension(const char* name, void* ptr);

/* SET_BPP(s, bpp) — sets channel bits/shifts for standard pixel formats.
 * The host derives BPP from these fields; there is no separate bpp field. */
#define SET_BPP(s, bpp_val) do { \
    if ((bpp_val) == 32) { \
        (s)->r_bits=8;(s)->r_shift=0; \
        (s)->g_bits=8;(s)->g_shift=8; \
        (s)->b_bits=8;(s)->b_shift=16; \
        (s)->a_bits=8;(s)->a_shift=24; \
    } else if ((bpp_val) == 16) { \
        (s)->r_bits=5;(s)->r_shift=11; \
        (s)->g_bits=6;(s)->g_shift=5; \
        (s)->b_bits=5;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
    } else if ((bpp_val) == 8) { \
        (s)->r_bits=3;(s)->r_shift=5; \
        (s)->g_bits=3;(s)->g_shift=2; \
        (s)->b_bits=2;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
    } \
} while(0)

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
} State;

static struct {
    State s;
    uint8_t vram[640 * 480 * 4];
} rom;

static int current_bpp = 16;
static int frame_phase = 0;
static int resize_state = 0;
static int initialized = 0;
static uint8_t keys_buf[256];
static uint8_t* keys = NULL;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int)rom.s.width || y < 0 || y >= (int)rom.s.height) return;
    int idx = y * (int)rom.s.width + x;
    if (current_bpp == 8) {
        uint8_t* fb = rom.vram;
        fb[idx] = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
    } else if (current_bpp == 16) {
        uint16_t* fb = (uint16_t*)rom.vram;
        fb[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else {
        uint32_t* fb = (uint32_t*)rom.vram;
        fb[idx] = (0xFF000000) | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear_screen(uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(0, 0, (int)rom.s.width, (int)rom.s.height, r, g, b);
}

static void draw_color_bars(void) {
    int w = (int)rom.s.width, h = (int)rom.s.height;
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
    int w = (int)rom.s.width, h = (int)rom.s.height;
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
    int w = (int)rom.s.width, h = (int)rom.s.height;
    fill_rect(0, h - 30, w, 30, 0, 0, 0);
    if (current_bpp == 8)
        fill_rect(5, h - 25, 20, 20, 255, 128, 0);
    else if (current_bpp == 16)
        fill_rect(5, h - 25, 20, 20, 0, 200, 255);
    else
        fill_rect(5, h - 25, 20, 20, 255, 255, 255);

    fill_rect(35, h - 25, 10, 10, 255, 255, 255);
    fill_rect(50, h - 25, 10, 10, 200, 200, 200);
}

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);

        keys = (uint8_t*)wextension("std:keyboard", keys_buf);

        initialized = 1;
    }

    frame_phase++;

    static int r_was_down = 0;
    static int key1_was_down = 0, key2_was_down = 0, key3_was_down = 0;

    int r_down = keys ? keys[21] : 0;
    if (r_down && !r_was_down) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) {
            rom.s.width = 320; rom.s.height = 240;
        } else if (resize_state == 1) {
            rom.s.width = 640; rom.s.height = 480;
        } else {
            rom.s.width = 160; rom.s.height = 120;
        }
    }
    r_was_down = r_down;

    int k1 = keys ? keys[30] : 0;
    if (k1 && !key1_was_down) current_bpp = 8;
    key1_was_down = k1;

    int k2 = keys ? keys[31] : 0;
    if (k2 && !key2_was_down) current_bpp = 16;
    key2_was_down = k2;

    int k3 = keys ? keys[32] : 0;
    if (k3 && !key3_was_down) current_bpp = 32;
    key3_was_down = k3;

    clear_screen(32, 32, 32);
    draw_color_bars();
    draw_grid();
    draw_status();

    int ax = (frame_phase * 3) % (int)rom.s.width;
    fill_rect(ax, 10, 20, 20, 255, 200, 0);

    SET_BPP(&rom.s, current_bpp);
    return (int)&rom.s;
}
