#include "piolho.h"
#include "framebuffer.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"
#include "gif.h"

static wframebuffer_t *framebuffer;
static wclock_t       *clock_ext;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;
static wgamepad_t     *gamepad;
static wgif_t         *gif;

static int initialized = 0;
static int test_passed = 0;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

int32_t wupdate(void) {
    if (!initialized) {
        // Test 1: Discover standard extensions via std:*
        framebuffer = (wframebuffer_t*)wextension(WFRAMEBUFFER_EXTENSION);
        clock_ext   = (wclock_t*)wextension(WCLOCK_EXTENSION);
        keyboard    = (wkeyboard_t*)wextension(WKEYBOARD_EXTENSION);
        mouse       = (wmouse_t*)wextension(WMOUSE_EXTENSION);
        gamepad     = (wgamepad_t*)wextension(WGAMEPAD_EXTENSION);
        gif         = (wgif_t*)wextension(WGIF_EXTENSION);

        // Test 2: Unknown extension returns NULL
        void* unk = wextension("unknown_custom_xyz");

        test_passed = (framebuffer != NULL) &&
                      (clock_ext != NULL) &&
                      (keyboard != NULL) &&
                      (mouse != NULL) &&
                      (gamepad != NULL) &&
                      (gif != NULL) &&
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

    return test_passed ? WUPDATE_OK : WUPDATE_ERROR;
}

