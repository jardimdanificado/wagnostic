// input_test — Tests all input methods per ABI spec
//
// Tests: keyboard (w_keys[256]), mouse (x/y/buttons/wheel), gamepad buttons.
// Layout: top=keyboard grid, bottom-left=mouse, bottom-right=gamepad.
// Mouse wheel shown as counter. Press ESC to quit.

#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

// === Globals per ABI spec ===
uint32_t w_width       = 320;
uint32_t w_height      = 240;
uint32_t w_bpp         = 16;
uint32_t w_scale       = 2;
char     w_title[128]  = "Input Test";
uint8_t  w_vram[320 * 240 * 2];
uint32_t w_dirty_count = 0;
Rect     w_dirty_rects[32];
int32_t  w_mouse_x     = 0;
int32_t  w_mouse_y     = 0;
uint32_t w_mouse_buttons = 0;
int32_t  w_mouse_wheel = 0;
uint8_t  w_keys[256]   = {0};
uint32_t w_gamepad_buttons = 0;
uint32_t w_ticks       = 0;

// === Helpers ===
static uint16_t* fb = (uint16_t*)w_vram;

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

// Simple digit renderer (5x7 font)
static const uint8_t font5x7[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
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
    char buf[12];
    int len = 0;
    int tmp = n;
    if (tmp < 0) { set_pixel(x, y, c); tmp = -tmp; x += 7; }
    while (tmp > 0 && len < 12) { buf[len++] = tmp % 10; tmp /= 10; }
    for (int i = len - 1; i >= 0; i--) {
        draw_digit(x, y, buf[i], c);
        x += 6;
    }
}

// === Draw sections ===

static void draw_keyboard_section(void) {
    // 16x16 grid of key states
    int cols = 16, rows = 16;
    int cell_w = 12, cell_h = 7;
    int ox = 8, oy = 5;

    fill_rect(ox - 2, oy - 2, cols * cell_w + 4, rows * cell_h + 4, rgb565(20, 20, 30));

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = ox + cx * cell_w, py = oy + cy * cell_h;
        uint16_t col = w_keys[i] ? rgb565(0, 220, 80) : rgb565(60, 60, 70);
        fill_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    // Label
    draw_number(ox, oy + rows * cell_h + 4, (int)w_ticks / 1000, rgb565(200, 200, 200));
}

static void draw_mouse_section(void) {
    int ox = 10, oy = 140;
    int w = 140, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));
    draw_vline(ox, oy, oy + h, rgb565(80, 80, 80));

    // Mouse position crosshair
    int mx = w_mouse_x, my = w_mouse_y;
    int cx = ox + 5 + (mx * (w - 10)) / 320;
    int cy = oy + 5 + (my * (h - 20)) / 240;
    draw_hline(cx - 8, cx + 8, cy, rgb565(255, 255, 255));
    draw_vline(cx, cy - 8, cy + 8, rgb565(255, 255, 255));
    fill_rect(cx - 1, cy - 1, 3, 3, rgb565(255, 0, 0));

    // Mouse buttons
    uint16_t lc = (w_mouse_buttons & 1) ? rgb565(255, 50, 50) : rgb565(80, 80, 80);
    uint16_t rc = (w_mouse_buttons & 2) ? rgb565(50, 50, 255) : rgb565(80, 80, 80);
    fill_rect(ox + 10, oy + h - 18, 25, 12, lc);
    fill_rect(ox + 40, oy + h - 18, 25, 12, rc);

    // Wheel counter
    draw_number(ox + 80, oy + h - 18, (int)w_mouse_wheel, rgb565(255, 255, 0));
}

static void draw_gamepad_section(void) {
    int ox = 170, oy = 140;
    int w = 145, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));

    // Gamepad button bits
    // bit0=up, bit1=down, bit2=left, bit3=right
    // bit4=a, bit5=b, bit6=select, bit7=start
    uint32_t gp = w_gamepad_buttons;

    int bx = ox + 10, by = oy + 10;
    // D-pad
    uint16_t dc = rgb565(100, 100, 100);
    fill_rect(bx + 10, by, 10, 10, (gp & 1) ? rgb565(0,255,0) : dc); // up
    fill_rect(bx + 10, by + 22, 10, 10, (gp & 2) ? rgb565(0,255,0) : dc); // down
    fill_rect(bx, by + 11, 10, 10, (gp & 4) ? rgb565(0,255,0) : dc); // left
    fill_rect(bx + 20, by + 11, 10, 10, (gp & 8) ? rgb565(0,255,0) : dc); // right
    fill_rect(bx + 10, by + 11, 10, 10, rgb565(50,50,50)); // center

    // Buttons A/B/Select/Start
    fill_rect(bx + 45, by + 5, 15, 15, (gp & 0x10) ? rgb565(255,50,50) : dc); // A
    fill_rect(bx + 65, by + 5, 15, 15, (gp & 0x20) ? rgb565(50,50,255) : dc); // B
    fill_rect(bx + 45, by + 25, 15, 10, (gp & 0x40) ? rgb565(200,200,0) : dc); // Select
    fill_rect(bx + 65, by + 25, 15, 10, (gp & 0x80) ? rgb565(200,200,0) : dc); // Start

    // Show raw value
    draw_number(bx, by + 50, (int)gp, rgb565(180, 180, 180));
}

int wupdate() {
    for (int i = 0; i < 320 * 240; i++) fb[i] = rgb565(15, 15, 20);

    draw_keyboard_section();
    draw_mouse_section();
    draw_gamepad_section();

    // Quit on ESC
    if (w_keys[41]) return 0;

    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, 320, 240};
    return 1;
}
