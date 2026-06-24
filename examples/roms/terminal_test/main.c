#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

#define RED   0xE0
#define GREEN 0x1C
#define BLUE  0x03
#define WHITE 0xFF
#define BLACK 0x00
#define GRAY  0x6D

uint32_t w_width       = 70;
uint32_t w_height      = 24;
uint32_t w_bpp         = 8;
uint32_t w_scale       = 1;
char w_title[128]      = "Terminal Test";
uint8_t w_vram[70 * 24 * 1];
uint32_t w_dirty_count = 0;
Rect w_dirty_rects[32];
int32_t w_mouse_x      = 0;
int32_t w_mouse_y      = 0;
uint32_t w_mouse_buttons = 0;
uint8_t w_keys[256]    = {0};
uint32_t w_ticks       = 0;

static void redraw() {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, (int)w_width, (int)w_height};
}

static void draw_rect(int x, int y, int w, int h, uint8_t color) {
    uint8_t* vram = (uint8_t*)w_vram;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int cur_x = x + i, cur_y = y + j;
            if (cur_x >= 0 && cur_x < 70 && cur_y >= 0 && cur_y < 24)
                vram[cur_y * 70 + cur_x] = color;
        }
    }
}

int wupdate() {
    uint8_t* vram = (uint8_t*)w_vram;

    for (int i = 0; i < 70 * 24; i++) vram[i] = BLACK;

    for (int i = 0; i < 26; i++) {
        int x = 2 + (i % 13) * 5, y = 5 + (i / 13) * 4;
        uint8_t color = (w_keys[4 + i]) ? GREEN : GRAY;
        draw_rect(x, y, 4, 3, color);
    }

    if (w_keys[41]) draw_rect(0, 0, 5, 2, RED);
    if (w_keys[40]) draw_rect(65, 0, 5, 2, BLUE);
    if (w_keys[42]) draw_rect(55, 0, 5, 2, 0xFC);
    if (w_keys[44]) draw_rect(20, 20, 30, 2, WHITE);

    int mx = w_mouse_x, my = w_mouse_y;
    uint8_t guide_color = (w_mouse_buttons & 1) ? RED : 0x24;
    for (int i = 0; i < 70; i++) vram[my * 70 + i] = guide_color;
    for (int i = 0; i < 24; i++) vram[i * 70 + mx] = guide_color;

    if (mx >= 0 && mx < 70 && my >= 0 && my < 24)
        vram[my * 70 + mx] = WHITE;

    redraw(); return 1;
}
