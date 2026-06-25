// full_test — Comprehensive test of ALL ABI features
//
// Split into 3 quadrants:
//   TL: Keyboard state grid (16x16)
//   TR: Dirty rectangle animation (tests multi-rect rendering)
//   BL: Mouse position + buttons + wheel
//
// Also tests: config changes (R=resize), BPP cycle (1/2/3), ESC=quit.
// Tests w_title, w_scale, w_bpp, w_ticks, all dirty rect modes.
// Audio globals are still declared (host configures audio) but the
// ROM does not feed the buffer — silent by design. See the audio_*
// ROMs for active audio playback.

#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

// === Globals per ABI spec ===
uint32_t w_width       = 320;
uint32_t w_height      = 240;
uint32_t w_bpp         = 16;
uint32_t w_scale       = 2;
char     w_title[128]  = "Full Test - 16bpp";
uint8_t  w_vram[320 * 240 * 4];
uint32_t w_dirty_count = 0;
Rect     w_dirty_rects[32];
int32_t  w_mouse_x     = 0;
int32_t  w_mouse_y     = 0;
uint32_t w_mouse_buttons = 0;
int32_t  w_mouse_wheel = 0;
uint8_t  w_keys[256]   = {0};
uint32_t w_gamepad_buttons = 0;
uint32_t w_ticks       = 0;

// Audio globals
uint32_t w_audio_size        = 8192;
uint32_t w_audio_sample_rate = 22050;
uint32_t w_audio_bpp         = 2;
uint32_t w_audio_channels    = 1;
uint32_t w_audio_write       = 0;
uint32_t w_audio_read        = 0;
uint8_t  w_audio_buffer[8192];
uint32_t w_audio_underrun  = 0;
uint32_t w_audio_overrun   = 0;

// === Internal state ===
static int current_bpp = 16;
static int frame_count = 0;
static int resize_state = 0;

// === Pixel operations ===
static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int)w_width || y < 0 || y >= (int)w_height) return;
    int idx = y * (int)w_width + x;
    if (current_bpp == 8) {
        w_vram[idx] = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
    } else if (current_bpp == 16) {
        ((uint16_t*)w_vram)[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else {
        ((uint32_t*)w_vram)[idx] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(0, 0, (int)w_width, (int)w_height, r, g, b);
}

// === 5x7 font ===
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

// === Quadrant: Keyboard (TL) ===
static void draw_keyboard(int ox, int oy, int qw, int qh) {
    int cols = 8, rows = 8;
    int cell_w = qw / (cols + 1), cell_h = qh / (rows + 2);
    int start_key = (frame_count / 120) % 3; // cycle which 64 keys shown

    for (int i = 0; i < 64; i++) {
        int key_idx = start_key * 64 + i;
        if (key_idx >= 256) break;
        int cx = i % cols, cy = i / cols;
        int px = ox + 4 + cx * cell_w, py = oy + 12 + cy * cell_h;
        uint8_t cr = w_keys[key_idx] ? 0 : 50;
        uint8_t cg = w_keys[key_idx] ? 200 : 50;
        uint8_t cb = w_keys[key_idx] ? 80 : 60;
        fill_rect(px, py, cell_w - 1, cell_h - 1, cr, cg, cb);
    }
}

// === Quadrant: Dirty Rect Animation (TR) ===
static int anim_x = 0, anim_y = 0, anim_dx = 2, anim_dy = 1;

static void draw_dirty_anim(int ox, int oy, int qw, int qh) {
    // Moving square
    anim_x += anim_dx; anim_y += anim_dy;
    if (anim_x <= 0 || anim_x + 15 >= qw) anim_dx = -anim_dx;
    if (anim_y <= 0 || anim_y + 15 >= qh) anim_dy = -anim_dy;

    // Background pattern (checkered)
    for (int y = 0; y < qh; y += 8)
        for (int x = 0; x < qw; x += 8)
            fill_rect(ox + x, oy + y, 7, 7,
                      ((x / 8 + y / 8) % 2) ? 40 : 25,
                      ((x / 8 + y / 8) % 2) ? 40 : 25,
                      ((x / 8 + y / 8) % 2) ? 50 : 35);

    // Animated square
    fill_rect(ox + anim_x, oy + anim_y, 15, 15, 255, 200, 0);

    // Trail effect
    fill_rect(ox + anim_x - anim_dx, oy + anim_y - anim_dy, 5, 5, 100, 80, 0);
}

// === Quadrant: Mouse (BL) ===
static void draw_mouse(int ox, int oy, int qw, int qh) {
    // Crosshair
    int mx = w_mouse_x, my = w_mouse_y;
    int cx = ox + (mx * qw) / (int)w_width;
    int cy = oy + (my * qh) / (int)w_height;

    // H and V lines
    for (int x = ox; x < ox + qw; x++) set_pixel(x, cy, 60, 60, 80);
    for (int y = oy; y < oy + qh; y++) set_pixel(cx, y, 60, 60, 80);

    // Cursor
    fill_rect(cx - 2, cy - 2, 5, 5, 255, 255, 255);

    // Left button indicator
    uint8_t lb = (w_mouse_buttons & 1) ? 255 : 80;
    fill_rect(ox + 2, oy + qh - 12, 15, 10, lb, 30, 30);

    // Right button indicator
    uint8_t rb = (w_mouse_buttons & 2) ? 100 : 80;
    fill_rect(ox + 22, oy + qh - 12, 15, 10, 30, 30, rb);

    // Wheel counter
    draw_number(ox + 45, oy + qh - 12, (int)w_mouse_wheel, 255, 255, 0);
}

// === Quadrant: Audio (BR) removed — see audio_wav/audio_mp3/audio_ogg
//     ROMs for active audio playback. Globals stay so the host still
//     configures audio (silent by design). ===

// === Config change handlers ===
static void update_title(void) {
    char* t = w_title;
    int i = 0;
    const char* prefix = "Full Test - ";
    while (*prefix) t[i++] = *prefix++;
    if (current_bpp == 8) { t[i++]='8'; t[i++]='b'; t[i++]='p'; }
    else if (current_bpp == 16) { t[i++]='1'; t[i++]='6'; t[i++]='b'; t[i++]='p'; }
    else { t[i++]='3'; t[i++]='2'; t[i++]='b'; t[i++]='p'; }
    t[i] = '\0';
}

int wupdate() {
    frame_count++;

    // === Input handling ===
    static int sp_was = 0, r_was = 0, k1_was = 0, k2_was = 0, k3_was = 0;

    // SPACE: cycle BPP
    if (w_keys[44] && !sp_was) {
        if (current_bpp == 8) current_bpp = 16;
        else if (current_bpp == 16) current_bpp = 32;
        else current_bpp = 8;
        w_bpp = current_bpp;
        update_title();
    }
    sp_was = w_keys[44];

    // R: resize
    if (w_keys[21] && !r_was) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) { w_width = 320; w_height = 240; w_scale = 2; }
        else if (resize_state == 1) { w_width = 640; w_height = 480; w_scale = 1; }
        else { w_width = 160; w_height = 120; w_scale = 4; }
    }
    r_was = w_keys[21];

    // 1/2/3: direct BPP
    if (w_keys[30] && !k1_was) { current_bpp = 8; w_bpp = 8; update_title(); }
    if (w_keys[31] && !k2_was) { current_bpp = 16; w_bpp = 16; update_title(); }
    if (w_keys[32] && !k3_was) { current_bpp = 32; w_bpp = 32; update_title(); }
    k1_was = w_keys[30]; k2_was = w_keys[31]; k3_was = w_keys[32];

    // ESC: quit
    if (w_keys[41]) return 0;

    // === Draw ===
    int W = (int)w_width, H = (int)w_height;
    clear(15, 15, 20);

    // Dividers
    fill_rect(W/2, 0, 1, H, 60, 60, 80);
    fill_rect(0, H/2, W, 1, 60, 60, 80);

    // Quadrants
    draw_keyboard(0, 0, W/2, H/2);
    draw_dirty_anim(W/2 + 1, 0, W/2 - 1, H/2);
    draw_mouse(0, H/2 + 1, W/2, H/2 - 1);

    // === Dirty rects: use multi-rect for the quadrants ===
    w_dirty_count = 3;
    w_dirty_rects[0] = (Rect){0, 0, W/2, H/2};
    w_dirty_rects[1] = (Rect){W/2, 0, W - W/2, H/2};
    w_dirty_rects[2] = (Rect){0, H/2, W/2, H - H/2};

    return 1;
}
