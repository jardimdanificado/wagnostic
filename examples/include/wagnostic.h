#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

/**
 * Wagnostic - WASM Multimedia Runtime
 * 
 * API based on named globals. ROMs export globals that the host reads/writes.
 * 
 * Usage:
 *   #define WAGNOSTIC_IMPLEMENTATION
 *   #include "wagnostic.h"
 */

#include <stdint.h>

// ============================================
// TYPES
// ============================================

#ifndef WAGNOSTIC_RECT_DEFINED
#define WAGNOSTIC_RECT_DEFINED
typedef struct {
    int x, y, w, h;
} Rect;
#endif

// ============================================
// GAMEPAD BUTTON CONSTANTS
// ============================================

#define W_BTN_UP     (1 << 0)
#define W_BTN_DOWN   (1 << 1)
#define W_BTN_LEFT   (1 << 2)
#define W_BTN_RIGHT  (1 << 3)
#define W_BTN_A      (1 << 4)
#define W_BTN_B      (1 << 5)
#define W_BTN_X      (1 << 6)
#define W_BTN_Y      (1 << 7)
#define W_BTN_L1     (1 << 8)
#define W_BTN_R1     (1 << 9)
#define W_BTN_START  (1 << 10)
#define W_BTN_SELECT (1 << 11)
#define W_BTN_L2     (1 << 12)
#define W_BTN_R2     (1 << 13)

// ============================================
// COLOR CONVERSION MACROS
// ============================================

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#define W_RGB332(r, g, b) (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

// ============================================
// DIRTY RECTANGLE CONSTANTS
// ============================================

#define W_MAX_DIRTY_RECTS 32

// ============================================
// GLOBAL DECLARATIONS
// ============================================

// --- Screen Configuration (ROM writes, Host reads) ---
extern uint32_t w_width;
extern uint32_t w_height;
extern uint32_t w_bpp;
extern uint32_t w_scale;
extern char w_title[128];

// --- VRAM (ROM writes, Host reads) ---
extern uint8_t w_vram[];

// --- Dirty Rectangles (ROM writes, Host reads) ---
extern uint32_t w_dirty_count;
extern Rect w_dirty_rects[W_MAX_DIRTY_RECTS];

// --- Input (Host writes, ROM reads) ---
extern int32_t w_mouse_x;
extern int32_t w_mouse_y;
extern uint32_t w_mouse_buttons;
extern int32_t w_mouse_wheel;
extern uint8_t w_keys[256];
extern uint32_t w_gamepad_buttons;

// --- Timing (Host writes, ROM reads) ---
extern uint32_t w_ticks;

// --- Audio (ROM writes, Host reads) ---
extern uint32_t w_audio_size;
extern uint32_t w_audio_sample_rate;
extern uint32_t w_audio_bpp;
extern uint32_t w_audio_channels;
extern uint32_t w_audio_write;
extern uint32_t w_audio_read;
extern uint8_t w_audio_buffer[];

// ============================================
// HELPER FUNCTIONS (inline)
// ============================================

static inline void w_setup(const char* title, int width, int height, int bpp, int scale, int signals_unused) {
    w_width = width;
    w_height = height;
    w_bpp = bpp;
    w_scale = scale;
    if (title) {
        int i = 0;
        while (title[i] && i < 127) {
            w_title[i] = title[i];
            i++;
        }
        w_title[i] = '\0';
    }
}

static inline void w_redraw() {
    w_dirty_count = 1;
    w_dirty_rects[0].x = 0;
    w_dirty_rects[0].y = 0;
    w_dirty_rects[0].w = w_width;
    w_dirty_rects[0].h = w_height;
}

static inline void w_redraw_rect(int x, int y, int w, int h) {
    if (w_dirty_count < W_MAX_DIRTY_RECTS) {
        w_dirty_rects[w_dirty_count].x = x;
        w_dirty_rects[w_dirty_count].y = y;
        w_dirty_rects[w_dirty_count].w = w;
        w_dirty_rects[w_dirty_count].h = h;
        w_dirty_count++;
    }
}

// ============================================
// CONVENIENCE MACROS
// ============================================

#define W_KEY_DOWN(scancode) (w_keys[scancode] != 0)
#define W_MOUSE_LEFT() ((w_mouse_buttons & 1) != 0)
#define W_MOUSE_RIGHT() ((w_mouse_buttons & 2) != 0)

// ============================================
// IMPLEMENTATION (only included once)
// ============================================

#ifdef WAGNOSTIC_IMPLEMENTATION

// --- Screen ---
__attribute__((used)) uint32_t w_width = 320;
__attribute__((used)) uint32_t w_height = 240;
__attribute__((used)) uint32_t w_bpp = 16;
__attribute__((used)) uint32_t w_scale = 4;
__attribute__((used)) char w_title[128] = "Wagnostic";

// --- VRAM ---
__attribute__((used)) uint8_t w_vram[320 * 240 * 4];

// --- Dirty Rectangles ---
__attribute__((used)) uint32_t w_dirty_count = 0;
__attribute__((used)) Rect w_dirty_rects[W_MAX_DIRTY_RECTS];

// --- Input ---
__attribute__((used)) int32_t w_mouse_x = 0;
__attribute__((used)) int32_t w_mouse_y = 0;
__attribute__((used)) uint32_t w_mouse_buttons = 0;
__attribute__((used)) int32_t w_mouse_wheel = 0;
__attribute__((used)) uint8_t w_keys[256] = {0};
__attribute__((used)) uint32_t w_gamepad_buttons = 0;

// --- Timing ---
__attribute__((used)) uint32_t w_ticks = 0;

// --- Audio ---
__attribute__((used)) uint32_t w_audio_size = 0;
__attribute__((used)) uint32_t w_audio_sample_rate = 44100;
__attribute__((used)) uint32_t w_audio_bpp = 16;
__attribute__((used)) uint32_t w_audio_channels = 1;
__attribute__((used)) uint32_t w_audio_write = 0;
__attribute__((used)) uint32_t w_audio_read = 0;
__attribute__((used)) uint8_t w_audio_buffer[8192];

#endif // WAGNOSTIC_IMPLEMENTATION

#endif // WAGNOSTIC_H