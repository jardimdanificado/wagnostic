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
            int stripe = (x / 40) % 2;
            fb[y * 320 + x] = stripe ? RGB(255, 255, 255) : RGB(0, 0, 0);
        }
    }

    int px = (ticks * 2) % 320;
    int py = 110;

    for (int sy = 0; sy < 20; sy++) {
        for (int sx = 0; sx < 20; sx++) {
            int cx = (px + sx) % 320;
            int cy = py + sy;
            fb[cy * 320 + cx] ^= 0x00FFFFFF;
        }
    }

    return WUPDATE_OK;
}
