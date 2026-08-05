typedef struct { int x, y, w, h; } Rect;
// input_test — Tests all input methods

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
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[552];
} State;

static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

static uint16_t* fb = (uint16_t*)rom.vram;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void set_pixel(int x, int y, uint16_t c) {
    if (x >= 0 && x < 320 && y >= 0 && y < 240)
        fb[y * 320 + x] = c;
}

static void fill_rect(int rx, int ry, int rw, int rh, uint16_t c) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, c);
}

static void draw_hline(int x1, int x2, int y, uint16_t c) {
    for (int x = x1; x < x2; x++) set_pixel(x, y, c);
}

static void draw_vline(int x, int y1, int y2, uint16_t c) {
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

static void draw_digit(int x, int y, int d, uint16_t c) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (font5x7[d][row] & (0x10 >> col))
                set_pixel(x + col, y + row, c);
}

static void draw_number(int x, int y, int n, uint16_t c) {
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

    fill_rect(ox - 2, oy - 2, cols * cell_w + 4, rows * cell_h + 4, rgb565(20, 20, 30));

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = ox + cx * cell_w, py = oy + cy * cell_h;
        uint16_t col = rom.s.keys[i] ? rgb565(0, 220, 80) : rgb565(60, 60, 70);
        fill_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    draw_number(ox, oy + rows * cell_h + 4, (int)rom.s.ticks / 1000, rgb565(200, 200, 200));
}

static void draw_mouse_section(void) {
    int ox = 10, oy = 140;
    int w = 140, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));
    draw_vline(ox, oy, oy + h, rgb565(80, 80, 80));

    int mx = rom.s.mouse_x, my = rom.s.mouse_y;
    int cx = ox + 5 + (mx * (w - 10)) / 320;
    int cy = oy + 5 + (my * (h - 20)) / 240;
    draw_hline(cx - 8, cx + 8, cy, rgb565(255, 255, 255));
    draw_vline(cx, cy - 8, cy + 8, rgb565(255, 255, 255));
    fill_rect(cx - 1, cy - 1, 3, 3, rgb565(255, 0, 0));

    uint16_t lc = (rom.s.mouse_buttons & 1) ? rgb565(255, 50, 50) : rgb565(80, 80, 80);
    uint16_t rc = (rom.s.mouse_buttons & 2) ? rgb565(50, 50, 255) : rgb565(80, 80, 80);
    fill_rect(ox + 10, oy + h - 18, 25, 12, lc);
    fill_rect(ox + 40, oy + h - 18, 25, 12, rc);

    draw_number(ox + 80, oy + h - 18, (int)rom.s.mouse_wheel, rgb565(255, 255, 0));
}

static void draw_gamepad_section(void) {
    int ox = 170, oy = 140;
    int w = 145, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));

    uint32_t gp = rom.s.gamepad_buttons;

    int bx = ox + 10, by = oy + 10;
    uint16_t dc = rgb565(100, 100, 100);
    fill_rect(bx + 10, by, 10, 10, (gp & 1) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 10, by + 22, 10, 10, (gp & 2) ? rgb565(0,255,0) : dc);
    fill_rect(bx, by + 11, 10, 10, (gp & 4) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 20, by + 11, 10, 10, (gp & 8) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 10, by + 11, 10, 10, rgb565(50,50,50));

    fill_rect(bx + 45, by + 5, 15, 15, (gp & 0x10) ? rgb565(255,50,50) : dc);
    fill_rect(bx + 65, by + 5, 15, 15, (gp & 0x20) ? rgb565(50,50,255) : dc);
    fill_rect(bx + 45, by + 25, 15, 10, (gp & 0x40) ? rgb565(200,200,0) : dc);
    fill_rect(bx + 65, by + 25, 15, 10, (gp & 0x80) ? rgb565(200,200,0) : dc);

    draw_number(bx, by + 50, (int)gp, rgb565(180, 180, 180));
}

static int initialized = 0;

int wupdate() {
    if (!initialized) {
        rom.s.width = 320;
        rom.s.height = 240;
        
        rom.s.scale = 2;
        rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
        char* t = rom.s.title;
        const char* src = "Input Test";
        int i = 0;
        while (src[i] && i < 127) { t[i] = src[i]; i++; }
        t[i] = '\0';
        initialized = 1;
    }

    for (int i = 0; i < 320 * 240; i++) fb[i] = rgb565(15, 15, 20);

    draw_keyboard_section();
    draw_mouse_section();
    draw_gamepad_section();

    if (rom.s.keys[41]) return 0;

    my_dirty_list.count = 1;
    my_dirty_list.rects[0] = (Rect){0, 0, 320, 240};
    SET_BPP(&rom.s, 16);
    rom.s.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&rom.s;
}
