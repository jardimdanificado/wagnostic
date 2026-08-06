typedef struct { int x, y, w, h; } Rect;
// full_test_dynamic — Test of dynamic VRAM allocation

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

typedef struct {
    int32_t x, y;
    uint32_t buttons;
    int32_t wheel;
} MouseState;

static State state;
static uint8_t* vram = 0;

static int current_bpp = 16;
static int frame_count = 0;
static int resize_state = 0;
static int initialized = 0;
static uint8_t keys_buf[256];
static MouseState mouse_buf;
static uint8_t* keys = NULL;
static MouseState* mouse = NULL;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= (int)state.width || y < 0 || y >= (int)state.height || !vram) return;
    int idx = y * (int)state.width + x;
    if (current_bpp == 8) {
        vram[idx] = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
    } else if (current_bpp == 16) {
        ((uint16_t*)vram)[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else {
        ((uint32_t*)vram)[idx] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear(uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(0, 0, (int)state.width, (int)state.height, r, g, b);
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
        int is_pressed = keys && keys[key_idx];
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
    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;
    int mwheel = mouse ? mouse->wheel : 0;

    int cx = ox + (mx * qw) / (int)state.width;
    int cy = oy + (my * qh) / (int)state.height;

    for (int x = ox; x < ox + qw; x++) set_pixel(x, cy, 60, 60, 80);
    for (int y = oy; y < oy + qh; y++) set_pixel(cx, y, 60, 60, 80);

    fill_rect(cx - 2, cy - 2, 5, 5, 255, 255, 255);

    uint8_t lb = (mbtns & 1) ? 255 : 80;
    fill_rect(ox + 2, oy + qh - 12, 15, 10, lb, 30, 30);

    uint8_t rb = (mbtns & 2) ? 100 : 80;
    fill_rect(ox + 22, oy + qh - 12, 15, 10, 30, 30, rb);

    draw_number(ox + 45, oy + qh - 12, mwheel, 255, 255, 0);
}

static void allocate_vram(uint32_t w, uint32_t h) {
    uint32_t max_bpp_bytes = 4;
    uint32_t required_bytes = w * h * max_bpp_bytes;
    uint32_t pages = (required_bytes + 65535) / 65536;
    
    uint32_t current_pages = __builtin_wasm_memory_size(0);
    
    if (!vram) {
        __builtin_wasm_memory_grow(0, pages);
        vram = (uint8_t*)(current_pages * 65536);
        state.vram_offset = (uint32_t)((uint8_t*)vram - (uint8_t*)&state);
    } else {
        uint32_t current_allocated_bytes = (__builtin_wasm_memory_size(0) - current_pages) * 65536;
        if (required_bytes > current_allocated_bytes) {
            __builtin_wasm_memory_grow(0, pages);
        }
    }
}

int wupdate() {
    if (!initialized) {
        state.width = 320;
        state.height = 240;
        allocate_vram(state.width, state.height);

        keys = (uint8_t*)wextension("std:keyboard", keys_buf);
        mouse = (MouseState*)wextension("std:mouse", &mouse_buf);

        initialized = 1;
    }

    frame_count++;

    static int sp_was = 0, r_was = 0, k1_was = 0, k2_was = 0, k3_was = 0;

    int key_sp = keys ? keys[44] : 0;
    if (key_sp && !sp_was) {
        if (current_bpp == 8) current_bpp = 16;
        else if (current_bpp == 16) current_bpp = 32;
        else current_bpp = 8;
    }
    sp_was = key_sp;

    int key_r = keys ? keys[21] : 0;
    if (key_r && !r_was) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) { state.width = 320; state.height = 240; }
        else if (resize_state == 1) { state.width = 640; state.height = 480; }
        else { state.width = 160; state.height = 120; }
        allocate_vram(state.width, state.height);
    }
    r_was = key_r;

    int k1 = keys ? keys[30] : 0;
    int k2 = keys ? keys[31] : 0;
    int k3 = keys ? keys[32] : 0;

    if (k1 && !k1_was) { current_bpp = 8; }
    if (k2 && !k2_was) { current_bpp = 16; }
    if (k3 && !k3_was) { current_bpp = 32; }
    k1_was = k1; k2_was = k2; k3_was = k3;

    if (keys && keys[41]) return 0;

    int W = (int)state.width, H = (int)state.height;
    clear(15, 15, 20);

    fill_rect(W/2, 0, 1, H, 60, 60, 80);
    fill_rect(0, H/2, W, 1, 60, 60, 80);

    draw_keyboard(0, 0, W/2, H/2);
    draw_dirty_anim(W/2 + 1, 0, W/2 - 1, H/2);
    draw_mouse(0, H/2 + 1, W/2, H/2 - 1);

    SET_BPP(&state, current_bpp);
    return (int)&state;
}
