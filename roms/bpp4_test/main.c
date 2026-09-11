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

    uint32_t *fb = (uint32_t*)surface->pixels;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int c = (x / 20) % 16;
            uint8_t r = (c & 1) ? 255 : ((c & 8) ? 128 : 0);
            uint8_t g = (c & 2) ? 255 : ((c & 8) ? 128 : 0);
            uint8_t b = (c & 4) ? 255 : ((c & 8) ? 128 : 0);
            fb[y * 320 + x] = RGB(r, g, b);
        }
    }

    return WUPDATE_OK;
}
