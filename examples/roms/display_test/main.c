// display_test — Tests all video modes and dirty rectangles
//
// Cycles through 8bpp (RGB332), 16bpp (RGB565), 32bpp (RGBA8888).
// Tests dirty rect modes: full-screen, single sub-rect, multiple rects.
// Press SPACE to cycle modes. Press 1/2/3 to jump to specific BPP.
// Press R to test config change (resize + title).
//
// Visual: color bars + grid pattern that changes per BPP mode.

#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

// === Globals per ABI spec ===
uint32_t w_width       = 320;
uint32_t w_height      = 240;
uint32_t w_bpp         = 16;
uint32_t w_scale       = 2;
char     w_title[128]  = "Display Test - 16bpp";
uint8_t  w_vram[320 * 240 * 4]; // max size for 32bpp
uint32_t w_dirty_count = 0;
Rect     w_dirty_rects[32];
int32_t  w_mouse_x     = 0;
int32_t  w_mouse_y     = 0;
uint32_t w_mouse_buttons = 0;
int32_t  w_mouse_wheel = 0;
uint8_t  w_keys[256]   = {0};
uint32_t w_ticks       = 0;

// === Internal state ===
static int current_bpp = 16;
static int dirty_mode = 0; // 0=full, 1=single sub-rect, 2=multi rects
static int frame_phase = 0;
static int resize_state = 0;

// BPP-aware pixel setters
static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int)w_width || y < 0 || y >= (int)w_height) return;
    int idx = y * (int)w_width + x;
    if (current_bpp == 8) {
        uint8_t* fb = w_vram;
        fb[idx] = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
    } else if (current_bpp == 16) {
        uint16_t* fb = (uint16_t*)w_vram;
        fb[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else {
        uint32_t* fb = (uint32_t*)w_vram;
        fb[idx] = (0xFF000000) | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear_screen(uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(0, 0, (int)w_width, (int)w_height, r, g, b);
}

// Draw color bars
static void draw_color_bars(void) {
    int w = (int)w_width, h = (int)w_height;
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

// Draw grid overlay
static void draw_grid(void) {
    int w = (int)w_width, h = (int)w_height;
    for (int x = 0; x < w; x += 32) {
        for (int y = 0; y < h; y++)
            set_pixel(x, y, 128, 128, 128);
    }
    for (int y = 0; y < h; y += 32) {
        for (int x = 0; x < w; x++)
            set_pixel(x, y, 128, 128, 128);
    }
}

// Draw status bar at bottom
static void draw_status(void) {
    int w = (int)w_width, h = (int)w_height;
    // Black bar
    fill_rect(0, h - 30, w, 30, 0, 0, 0);
    // BPP indicator — colored rectangle
    if (current_bpp == 8)
        fill_rect(5, h - 25, 20, 20, 255, 128, 0);
    else if (current_bpp == 16)
        fill_rect(5, h - 25, 20, 20, 0, 200, 255);
    else
        fill_rect(5, h - 25, 20, 20, 255, 255, 255);

    // Dirty mode indicator
    uint8_t dr = (dirty_mode == 0) ? 255 : 0;
    uint8_t dg = (dirty_mode == 1) ? 255 : 0;
    uint8_t db = (dirty_mode == 2) ? 255 : 0;
    fill_rect(35, h - 25, 10, 10, dr, dg, db);
    fill_rect(50, h - 25, 10, 10, 200, 200, 200);
}

static void redraw_full(void) {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, (int)w_width, (int)w_height};
}

static void redraw_subrect(int rx, int ry, int rw, int rh) {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){rx, ry, rw, rh};
}

static void redraw_multi(void) {
    int w = (int)w_width, h = (int)w_height;
    w_dirty_count = 4;
    w_dirty_rects[0] = (Rect){0, 0, w/2, h/2};
    w_dirty_rects[1] = (Rect){w/2, 0, w - w/2, h/2};
    w_dirty_rects[2] = (Rect){0, h/2, w/2, h - h/2};
    w_dirty_rects[3] = (Rect){w/2, h/2, w - w/2, h - h/2};
}

int wupdate() {
    frame_phase++;

    // Handle input: SPACE cycles dirty mode, 1/2/3 sets BPP, R triggers resize
    static int space_was_down = 0;
    static int r_was_down = 0;
    static int key1_was_down = 0, key2_was_down = 0, key3_was_down = 0;

    int space_down = w_keys[44]; // Space
    if (space_down && !space_was_down) {
        dirty_mode = (dirty_mode + 1) % 3;
    }
    space_was_down = space_down;

    int r_down = w_keys[21]; // R
    if (r_down && !r_was_down) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) {
            w_width = 320; w_height = 240; w_scale = 2;
            // Restore original title
            char* t = w_title;
            t[0]='D'; t[1]='i'; t[2]='s'; t[3]='p'; t[4]='l'; t[5]='a';
            t[6]='y'; t[7]=' '; t[8]='T'; t[9]='e'; t[10]='s'; t[11]='t';
            t[12]=' '; t[13]='-'; t[14]=' ';
            if (current_bpp==8) { t[15]='8'; t[16]='b'; t[17]='p'; t[18]='p'; }
            else if (current_bpp==16) { t[15]='1'; t[16]='6'; t[17]='b'; t[18]='p'; t[19]='p'; }
            else { t[15]='3'; t[16]='2'; t[17]='b'; t[18]='p'; t[19]='p'; }
            t[20]='\0';
        } else if (resize_state == 1) {
            w_width = 640; w_height = 480; w_scale = 1;
            char* t = w_title;
            t[0]='R'; t[1]='e'; t[2]='s'; t[3]='i'; t[4]='z'; t[5]='e';
            t[6]='d'; t[7]=' '; t[8]='6'; t[9]='4'; t[10]='0'; t[11]='x';
            t[12]='4'; t[13]='8'; t[14]='0'; t[15]='\0';
        } else {
            w_width = 160; w_height = 120; w_scale = 4;
            char* t = w_title;
            t[0]='S'; t[1]='m'; t[2]='a'; t[3]='l'; t[4]='l'; t[5]=' ';
            t[6]='1'; t[7]='6'; t[8]='0'; t[9]='x'; t[10]='1'; t[11]='2';
            t[12]='0'; t[13]='\0';
        }
    }
    r_was_down = r_down;

    int k1 = w_keys[30]; // Key 1
    if (k1 && !key1_was_down) current_bpp = 8;
    key1_was_down = k1;

    int k2 = w_keys[31]; // Key 2
    if (k2 && !key2_was_down) current_bpp = 16;
    key2_was_down = k2;

    int k3 = w_keys[32]; // Key 3
    if (k3 && !key3_was_down) current_bpp = 32;
    key3_was_down = k3;

    // Update w_bpp to match
    w_bpp = current_bpp;

    // Draw
    clear_screen(32, 32, 32);
    draw_color_bars();
    draw_grid();
    draw_status();

    // Animated element to show updates
    int ax = (frame_phase * 3) % (int)w_width;
    fill_rect(ax, 10, 20, 20, 255, 200, 0);

    // Set dirty rects based on mode
    if (dirty_mode == 0) {
        redraw_full();
    } else if (dirty_mode == 1) {
        // Just the animated element + status bar
        redraw_subrect(0, 0, (int)w_width, (int)w_height);
    } else {
        redraw_multi();
    }

    return 1;
}
