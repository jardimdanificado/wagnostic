#include "wagnostic.h"

static uint16_t* _fb;

__attribute__((visibility("default")))
void winit() {
    w_setup("Wagnostic - Buttons & Mouse Test", 320, 240, 16, 4, 0);
    _fb = (uint16_t*)W_FB_PTR;
}

void draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= 240) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < 320) {
                _fb[iy * 320 + ix] = color;
            }
        }
    }
}

__attribute__((visibility("default")))
void wupdate() {
    // Fill screen with background
    for (int i = 0; i < 320 * 240; i++) _fb[i] = W_RGB565(51, 51, 51);
    
    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (320 - (cols * cell_w)) / 2;
    int margin_y = (240 - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        
        uint16_t col = W_RGB565(119, 119, 119);
        if (W_SYS->keys[i]) col = W_RGB565(0, 204, 85);
        
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }
    
    // Draw mouse cursor
    draw_rect(W_SYS->mouse_x - 2, W_SYS->mouse_y - 2, 5, 5, W_RGB565(255, 255, 255));
    if (W_SYS->mouse_buttons & 1) draw_rect(W_SYS->mouse_x - 4, W_SYS->mouse_y - 4, 9, 9, W_RGB565(255, 0, 0));

    w_redraw();
}