#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"
#include "image32.h"

static Olivec_Canvas _oc;
static Olivec_Canvas _img;

void winit() {
    w_setup("Wagnostic Test - 32bpp", 320, 240, 32, 2, 4);
    _oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 32);
    _img = olivec_canvas((uint32_t*)image32_raw, image32_width, image32_height, image32_width, 32);
}

__attribute__((visibility("default")))
void wupdate() {
    olivec_fill(_oc, 0);
    int x = (320 - (int)image32_width) / 2;
    int y = (240 - (int)image32_height) / 2;
    olivec_sprite_copy(_oc, x, y, image32_width, image32_height, _img);
    olivec_text(_oc, "32BPP MODE (RGBA)", 10, 10, olivec_default_font, 2, 0xFFFFFFFF);
    w_redraw();
}
