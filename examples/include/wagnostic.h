#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

/**
 * Wagnostic - WASM Multimedia Runtime
 * 
 * New API: ROMs export named global variables that the host reads/writes.
 * No fixed memory offsets. No special compilation flags needed.
 * Works with any language that compiles to WASM.
 * 
 * Usage:
 *   #define WAGNOSTIC_IMPLEMENTATION
 *   #include "wagnostic.h"
 */

#include <stdint.h>

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
// SIGNAL CONSTANTS
// ============================================

#define W_SIG_REDRAW        1
#define W_SIG_QUIT          2
#define W_SIG_UPDATE_TITLE  3
#define W_SIG_UPDATE_WINDOW 4
#define W_SIG_UPDATE_AUDIO  5
#define W_SIG_LOG_INFO      6

// ============================================
// COLOR CONVERSION MACROS
// ============================================

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#define W_RGB332(r, g, b) (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

// ============================================
// GLOBAL DECLARATIONS
// ============================================

// --- Screen Configuration (ROM writes, host reads) ---
extern uint32_t w_width;
extern uint32_t w_height;
extern uint32_t w_bpp;
extern uint32_t w_scale;
extern char w_title[128];

// --- VRAM (ROM writes, host reads) ---
extern uint8_t w_vram[];

// --- Input (host writes, ROM reads) ---
extern int32_t w_mouse_x;
extern int32_t w_mouse_y;
extern uint32_t w_mouse_buttons;
extern int32_t w_mouse_wheel;
extern uint8_t w_keys[256];
extern uint32_t w_gamepad_buttons;

// --- Timing (host writes, ROM reads) ---
extern uint32_t w_ticks;

// --- Audio (ROM writes, host reads) ---
extern uint32_t w_audio_size;
extern uint32_t w_audio_sample_rate;
extern uint32_t w_audio_bpp;
extern uint32_t w_audio_channels;
extern uint32_t w_audio_write;
extern uint32_t w_audio_read;
extern uint8_t w_audio_buffer[];

// --- Signals (ROM writes, host reads) ---
extern uint8_t w_signal_redraw;
extern uint8_t w_signal_quit;
extern uint8_t w_signal_update_window;
extern uint8_t w_signal_update_audio;

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
    w_signal_redraw = 1;
}

static inline void* w_audio_ptr() {
    return w_audio_buffer;
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
uint32_t w_width = 320;
uint32_t w_height = 240;
uint32_t w_bpp = 16;
uint32_t w_scale = 4;
char w_title[128] = "Wagnostic";

// --- VRAM ---
uint8_t w_vram[320 * 240 * 2];

// --- Input ---
int32_t w_mouse_x = 0;
int32_t w_mouse_y = 0;
uint32_t w_mouse_buttons = 0;
int32_t w_mouse_wheel = 0;
uint8_t w_keys[256] = {0};
uint32_t w_gamepad_buttons = 0;

// --- Timing ---
uint32_t w_ticks = 0;

// --- Audio ---
uint32_t w_audio_size = 0;
uint32_t w_audio_sample_rate = 44100;
uint32_t w_audio_bpp = 16;
uint32_t w_audio_channels = 1;
uint32_t w_audio_write = 0;
uint32_t w_audio_read = 0;
uint8_t w_audio_buffer[8192];

// --- Signals ---
uint8_t w_signal_redraw = 0;
uint8_t w_signal_quit = 0;
uint8_t w_signal_update_window = 0;
uint8_t w_signal_update_audio = 0;

#endif // WAGNOSTIC_IMPLEMENTATION

#endif // WAGNOSTIC_H