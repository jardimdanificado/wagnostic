#include "piolho.h"
#include "framebuffer.h"

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static wframebuffer_t *surface;
static int initialized = 0;
static uint32_t ticks = 0;

int32_t wupdate(void) {
    ticks++;
    if (!initialized) {
        surface = (wframebuffer_t*)wextension(WFRAMEBUFFER_EXTENSION);
        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }
        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint32_t* fb = (uint32_t*)surface->pixels;
    static uint32_t last_tick = 0;
    static uint32_t color = RGB(0, 0, 255);

    if (ticks - last_tick > 60) {
        color = (color == RGB(0, 0, 255)) ? RGB(255, 0, 0) : ((color == RGB(255, 0, 0)) ? RGB(0, 255, 0) : RGB(0, 0, 255));
        last_tick = ticks;
    }

    for (int i = 0; i < 320 * 240; i++)
        fb[i] = color;

    return WUPDATE_OK;
}
