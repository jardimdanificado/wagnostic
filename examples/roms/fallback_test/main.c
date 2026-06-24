#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

// No winit — globals initialized at definition with defaults above.

uint32_t w_width     = 320;
uint32_t w_height    = 240;
uint32_t w_bpp       = 8;
uint32_t w_scale     = 1;
char w_title[128]    = "Fallback Test";
uint8_t w_vram[320 * 240 * 1];
uint32_t w_dirty_count = 0;
Rect w_dirty_rects[32];
int32_t w_mouse_x      = 0;
int32_t w_mouse_y      = 0;
uint32_t w_mouse_buttons = 0;
uint32_t w_ticks       = 0;
uint8_t w_keys[256]    = {0};

static void redraw() {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, (int)w_width, (int)w_height};
}

int wupdate() {
    uint8_t* fb = (uint8_t*)w_vram;
    static uint32_t last_tick = 0;
    static uint8_t color = 0;

    uint32_t now = w_ticks;
    if (now - last_tick > 1000) {
        color += 32;
        last_tick = now;
    }

    for (int i = 0; i < 320 * 240; i++)
        fb[i] = color;

    redraw(); return 1;
}
