#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"
#include "image.h"

static Olivec_Canvas _oc;
static Olivec_Canvas _img;
static int pos_x = 100, pos_y = 70;
static int scale_w = 120, scale_h = 100;

void winit() {
    w_setup("Wagnostic SDK - Images", 320, 240, 16, 4, 8);
    _oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 16);
    _img = olivec_canvas((uint16_t*)image_raw, image_width, image_height, image_width, 16);
}

__attribute__((visibility("default")))
void wupdate() {
    olivec_fill(_oc, 0);

    if (W_SYS->gamepad_buttons & W_BTN_LEFT)  pos_x -= 2;
    if (W_SYS->gamepad_buttons & W_BTN_RIGHT) pos_x += 2;
    if (W_SYS->gamepad_buttons & W_BTN_UP)    pos_y -= 2;
    if (W_SYS->gamepad_buttons & W_BTN_DOWN)  pos_y += 2;
    if (W_SYS->gamepad_buttons & W_BTN_L1)    scale_w -= 2;
    if (W_SYS->gamepad_buttons & W_BTN_R1)    scale_w += 2;
    if (W_SYS->gamepad_buttons & W_BTN_L2)    scale_h -= 2;
    if (W_SYS->gamepad_buttons & W_BTN_R2)    scale_h += 2;
    if (W_SYS->gamepad_buttons & W_BTN_A)     { scale_w = image_width; scale_h = image_height; }

    olivec_sprite_copy(_oc, pos_x, pos_y, scale_w, scale_h, _img);
    w_redraw();
}
