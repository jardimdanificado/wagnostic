#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"
#include "image8.h"

static Olivec_Canvas _oc;
static Olivec_Canvas _img;

void winit() {
    w_setup("Wagnostic Test - 8bpp", 320, 240, 8, 2, 4);
    _oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 8);
    _img = olivec_canvas((uint8_t*)image8_raw, image8_width, image8_height, image8_width, 8);
}

__attribute__((visibility("default")))
void wupdate() {
    olivec_fill(_oc, 0);
    // Centraliza a imagem
    int x = (320 - (int)image8_width) / 2;
    int y = (240 - (int)image8_height) / 2;
    olivec_sprite_copy(_oc, x, y, image8_width, image8_height, _img);
    olivec_text(_oc, "8BPP MODE (RGB332)", 10, 10, olivec_default_font, 2, 0xFF);
    w_redraw();
}
