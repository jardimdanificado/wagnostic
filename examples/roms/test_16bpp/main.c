#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"
#include "image.rgb565.h"

static Olivec_Canvas _oc;
static Olivec_Canvas _img;

void winit() {
    w_setup("Wagnostic Test - 16bpp", 320, 240, 16, 2, 4);
    _oc = olivec_canvas(w_vram, 320, 240, 320, 16);
    _img = olivec_canvas((uint16_t*)image_pixels, image_width, image_height, image_width, 16);
}

__attribute__((visibility("default")))
void wupdate() {
    olivec_fill(_oc, 0);
    int x = (320 - (int)image_width) / 2;
    int y = (240 - (int)image_height) / 2;
    olivec_sprite_copy(_oc, x, y, image_width, image_height, _img);
    olivec_text(_oc, "16BPP MODE (RGB565)", 10, 10, olivec_default_font, 2, 0xFFFF);
    w_redraw();
}
