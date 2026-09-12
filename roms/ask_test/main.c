#include "wagnostic.h"

// Structs declared directly from STD.md documentation
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixels;
} wframebuffer_t;

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    float    delta;
} wclock_t;

typedef struct {
    uint8_t keys[256];
} wkeyboard_t;

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t buttons;
    int32_t  wheel_x;
    int32_t  wheel_y;
} wmouse_t;

static wframebuffer_t *framebuffer;
static wclock_t       *clock_ext;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;

static int initialized = 0;
static int test_passed = 0;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

int32_t update(void) {
    if (!initialized) {
        // Discover standard extensions by string name
        framebuffer = (wframebuffer_t*)ask("std:framebuffer");
        clock_ext   = (wclock_t*)ask("std:clock");
        keyboard    = (wkeyboard_t*)ask("std:keyboard");
        mouse       = (wmouse_t*)ask("std:mouse");

        void* unk = ask("unknown_custom_xyz");

        test_passed = (framebuffer != NULL) &&
                      (clock_ext != NULL) &&
                      (keyboard != NULL) &&
                      (mouse != NULL) &&
                      (unk == NULL) &&
                      (framebuffer->width == 320) &&
                      (framebuffer->height == 240);

        initialized = 1;
    }

    if (framebuffer && framebuffer->pixels) {
        uint32_t color = test_passed ? RGB(30, 180, 50) : RGB(200, 30, 30);
        uint32_t *fb = (uint32_t*)framebuffer->pixels;
        uint32_t w = framebuffer->width ? framebuffer->width : 320;
        uint32_t h = framebuffer->height ? framebuffer->height : 240;

        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                fb[y * w + x] = color;
            }
        }
    }

    return test_passed ? UPDATE_OK : UPDATE_ERROR;
}
