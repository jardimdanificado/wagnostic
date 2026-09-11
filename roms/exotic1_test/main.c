#include "piolho.h"
#include "framebuffer.h"

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

static framebuffer_t *surface;
static int initialized = 0;
static uint32_t ticks = 0;

int32_t update(void) {
    ticks++;
    if (!initialized) {
        surface = (framebuffer_t*)use(FRAMEBUFFER_EXTENSION);
        if (surface) {
            surface->width = 320;
            surface->height = 240;
        }
        initialized = 1;
    }

    if (!surface || !surface->pixels) return UPDATE_ERROR;

    uint32_t *fb = (uint32_t*)surface->pixels;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t r = (uint8_t)(x * 255 / 320);
            uint8_t g = (uint8_t)(y * 255 / 240);
            uint8_t b = (uint8_t)((ticks * 4) & 0xFF);
            fb[y * 320 + x] = RGB(r, g, b);
        }
    }

    return UPDATE_OK;
}
