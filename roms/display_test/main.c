typedef struct { int x, y, w, h; } Rect;
// display_test — Tests all video modes and dirty rectangles

#include <stdint.h>

/* SET_BPP(s, bpp) — sets channel bits/shifts for standard pixel formats.
 * The host derives BPP from these fields; there is no separate bpp field. */
#define SET_BPP(s, bpp_val) do { \
    if ((bpp_val) == 32) { \
        (s)->r_bits=8;(s)->r_shift=0; \
        (s)->g_bits=8;(s)->g_shift=8; \
        (s)->b_bits=8;(s)->b_shift=16; \
        (s)->a_bits=8;(s)->a_shift=24; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } else if ((bpp_val) == 16) { \
        (s)->r_bits=5;(s)->r_shift=11; \
        (s)->g_bits=6;(s)->g_shift=5; \
        (s)->b_bits=5;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } else if ((bpp_val) == 8) { \
        (s)->r_bits=3;(s)->r_shift=5; \
        (s)->g_bits=3;(s)->g_shift=2; \
        (s)->b_bits=2;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
        (s)->x_bits=0;(s)->x_shift=0; \
    } \
} while(0)





typedef struct {
    uint32_t width, height, scale;
    char title[128];
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint8_t reserved[516];
} State;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[640 * 480 * 4];
} rom;

static int current_bpp = 16;
static int dirty_mode = 0;
static int frame_phase = 0;
static int resize_state = 0;
static int initialized = 0;

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

    uint8_t dr = (dirty_mode == 0) ? 255 : 0;
    uint8_t dg = (dirty_mode == 1) ? 255 : 0;
    uint8_t db = (dirty_mode == 2) ? 255 : 0;
    fill_rect(35, h - 25, 10, 10, dr, dg, db);
    fill_rect(50, h - 25, 10, 10, 200, 200, 200);
}

static void redraw_full(void) {
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, (int)rom.s.width, (int)rom.s.height};
}

static void redraw_subrect(int rx, int ry, int rw, int rh) {
    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){rx, ry, rw, rh};
}

static void redraw_multi(void) {
    int w = (int)rom.s.width, h = (int)rom.s.height;
    my_dirty_list.count = 4;
    my_dirty_list.rects[0] = (Rect){0, 0, w/2, h/2};
    my_dirty_list.rects[1] = (Rect){w/2, 0, w - w/2, h/2};
    my_dirty_list.rects[2] = (Rect){0, h/2, w/2, h - h/2};
    my_dirty_list.rects[3] = (Rect){w/2, h/2, w - w/2, h - h/2};
}

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        char* t = rom.s.title;
        const char* src = "Display Test - 16bpp";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        initialized = 1;
    }

    frame_phase++;

    static int space_was_down = 0;
    static int r_was_down = 0;
    static int key1_was_down = 0, key2_was_down = 0, key3_was_down = 0;

    int space_down = rom.s.keys[44];
    if (space_down && !space_was_down) {
        dirty_mode = (dirty_mode + 1) % 3;
    }
    space_was_down = space_down;

    int r_down = rom.s.keys[21];
    if (r_down && !r_was_down) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) {
            rom.s.width = 320; rom.s.height = 240; rom.s.scale = 2;
            char* t = rom.s.title;
            t[0]='D'; t[1]='i'; t[2]='s'; t[3]='p'; t[4]='l'; t[5]='a';
            t[6]='y'; t[7]=' '; t[8]='T'; t[9]='e'; t[10]='s'; t[11]='t';
            t[12]=' '; t[13]='-'; t[14]=' ';
            if (current_bpp==8) { t[15]='8'; t[16]='b'; t[17]='p'; t[18]='p'; }
            else if (current_bpp==16) { t[15]='1'; t[16]='6'; t[17]='b'; t[18]='p'; t[19]='p'; }
            else { t[15]='3'; t[16]='2'; t[17]='b'; t[18]='p'; t[19]='p'; }
            t[20]='\0';
        } else if (resize_state == 1) {
            rom.s.width = 640; rom.s.height = 480; rom.s.scale = 1;
            char* t = rom.s.title;
            t[0]='R'; t[1]='e'; t[2]='s'; t[3]='i'; t[4]='z'; t[5]='e';
            t[6]='d'; t[7]=' '; t[8]='6'; t[9]='4'; t[10]='0'; t[11]='x';
            t[12]='4'; t[13]='8'; t[14]='0'; t[15]='\0';
        } else {
            rom.s.width = 160; rom.s.height = 120; rom.s.scale = 4;
            char* t = rom.s.title;
            t[0]='S'; t[1]='m'; t[2]='a'; t[3]='l'; t[4]='l'; t[5]=' ';
            t[6]='1'; t[7]='6'; t[8]='0'; t[9]='x'; t[10]='1'; t[11]='2';
            t[12]='0'; t[13]='\0';
        }
    }
    r_was_down = r_down;

    int k1 = rom.s.keys[30];
    if (k1 && !key1_was_down) current_bpp = 8;
    key1_was_down = k1;

    int k2 = rom.s.keys[31];
    if (k2 && !key2_was_down) current_bpp = 16;
    key2_was_down = k2;

    int k3 = rom.s.keys[32];
    if (k3 && !key3_was_down) current_bpp = 32;
    key3_was_down = k3;


    clear_screen(32, 32, 32);
    draw_color_bars();
    draw_grid();
    draw_status();

    int ax = (frame_phase * 3) % (int)rom.s.width;
    fill_rect(ax, 10, 20, 20, 255, 200, 0);

    if (dirty_mode == 0) {
        redraw_full();
    } else if (dirty_mode == 1) {
        redraw_subrect(0, 0, (int)rom.s.width, (int)rom.s.height);
    } else {
        redraw_multi();
    }

    SET_BPP(&rom.s, current_bpp);
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
