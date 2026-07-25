typedef struct { int x, y, w, h; } Rect;
// full_test — Comprehensive test of ALL ABI features

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
static int frame_count = 0;
static int resize_state = 0;
static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int)rom.s.width || y < 0 || y >= (int)rom.s.height) return;
    int idx = y * (int)rom.s.width + x;
    if (current_bpp == 8) {
        rom.vram[idx] = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
    } else if (current_bpp == 16) {
        ((uint16_t*)rom.vram)[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else {
        ((uint32_t*)rom.vram)[idx] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(0, 0, (int)rom.s.width, (int)rom.s.height, r, g, b);
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
        uint8_t cr = rom.s.keys[key_idx] ? 0 : 50;
        uint8_t cg = rom.s.keys[key_idx] ? 200 : 50;
        uint8_t cb = rom.s.keys[key_idx] ? 80 : 60;
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
    int mx = rom.s.mouse_x, my = rom.s.mouse_y;
    int cx = ox + (mx * qw) / (int)rom.s.width;
    int cy = oy + (my * qh) / (int)rom.s.height;

    for (int x = ox; x < ox + qw; x++) set_pixel(x, cy, 60, 60, 80);
    for (int y = oy; y < oy + qh; y++) set_pixel(cx, y, 60, 60, 80);

    fill_rect(cx - 2, cy - 2, 5, 5, 255, 255, 255);

    uint8_t lb = (rom.s.mouse_buttons & 1) ? 255 : 80;
    fill_rect(ox + 2, oy + qh - 12, 15, 10, lb, 30, 30);

    uint8_t rb = (rom.s.mouse_buttons & 2) ? 100 : 80;
    fill_rect(ox + 22, oy + qh - 12, 15, 10, 30, 30, rb);

    draw_number(ox + 45, oy + qh - 12, (int)rom.s.mouse_wheel, 255, 255, 0);
}

static void update_title(void) {
    char* t = rom.s.title;
    int i = 0;
    const char* prefix = "Full Test - ";
    while (*prefix) t[i++] = *prefix++;
    if (current_bpp == 8) { t[i++]='8'; t[i++]='b'; t[i++]='p'; }
    else if (current_bpp == 16) { t[i++]='1'; t[i++]='6'; t[i++]='b'; t[i++]='p'; }
    else { t[i++]='3'; t[i++]='2'; t[i++]='b'; t[i++]='p'; }
    t[i] = '\0';
}

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        
        rom.s.scale = 2;
        rom.s.audio_size = 8192;
        rom.s.audio_sample_rate = 22050;
        rom.s.audio_bpp = 2;
        rom.s.audio_channels = 1;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        update_title();
        initialized = 1;
    }

    frame_count++;

    static int sp_was = 0, r_was = 0, k1_was = 0, k2_was = 0, k3_was = 0;

    if (rom.s.keys[44] && !sp_was) {
        if (current_bpp == 8) current_bpp = 16;
        else if (current_bpp == 16) current_bpp = 32;
        else current_bpp = 8;
        update_title();
    }
    sp_was = rom.s.keys[44];

    if (rom.s.keys[21] && !r_was) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) { rom.s.width = 320; rom.s.height = 240; rom.s.scale = 2; }
        else if (resize_state == 1) { rom.s.width = 640; rom.s.height = 480; rom.s.scale = 1; }
        else { rom.s.width = 160; rom.s.height = 120; rom.s.scale = 4; }
    }
    r_was = rom.s.keys[21];

    if (rom.s.keys[30] && !k1_was) { current_bpp = 8;  update_title(); }
    if (rom.s.keys[31] && !k2_was) { current_bpp = 16;  update_title(); }
    if (rom.s.keys[32] && !k3_was) { current_bpp = 32;  update_title(); }
    k1_was = rom.s.keys[30]; k2_was = rom.s.keys[31]; k3_was = rom.s.keys[32];

    if (rom.s.keys[41]) return 0;

    int W = (int)rom.s.width, H = (int)rom.s.height;
    clear(15, 15, 20);

    fill_rect(W/2, 0, 1, H, 60, 60, 80);
    fill_rect(0, H/2, W, 1, 60, 60, 80);

    draw_keyboard(0, 0, W/2, H/2);
    draw_dirty_anim(W/2 + 1, 0, W/2 - 1, H/2);
    draw_mouse(0, H/2 + 1, W/2, H/2 - 1);

    my_dirty_list.count = 3;
    my_dirty_list.rects[0] = (Rect){0, 0, W/2, H/2};
    my_dirty_list.rects[1] = (Rect){W/2, 0, W - W/2, H/2};
    my_dirty_list.rects[2] = (Rect){0, H/2, W/2, H - H/2};

    SET_BPP(&rom.s, current_bpp);
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
