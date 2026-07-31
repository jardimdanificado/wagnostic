#ifndef GIFNOSTIC_H
#define GIFNOSTIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Wagnostic State Struct (1024 bytes ABI guarantee)
 * ================================================================ */

typedef struct {
    int32_t x, y, w, h;
} WagnosticRect;

typedef struct {
    uint32_t width, height, scale;
    char title[128];
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t audio_chunk_samples, audio_volume, audio_paused;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[500];
} WagnosticState;

static_assert(sizeof(WagnosticState) == 1024, "WagnosticState must be exactly 1024 bytes");

/* Opaque context handle */
typedef struct WagnosticContext WagnosticContext;

/* ================================================================
 * Gifnostic Headless API
 * ================================================================ */

/* Create context from WASM byte buffer */
WagnosticContext* wagnostic_create(const uint8_t* wasm_bytes, size_t wasm_size, uint32_t stack_size_bytes);

/* Create context from file path (.wasm or .wag/.tar archive containing main.wasm) */
WagnosticContext* wagnostic_create_from_file(const char* file_path, uint32_t stack_size_bytes);

/* Destroy context and free resources */
void wagnostic_destroy(WagnosticContext* ctx);

/* Step single frame (calls wupdate). Returns 1 to continue, 0 to stop/quit/error */
int wagnostic_step(WagnosticContext* ctx);

/* Getters for linear memory objects */
WagnosticState* wagnostic_get_state(WagnosticContext* ctx);
uint8_t* wagnostic_get_vram(WagnosticContext* ctx);
uint8_t* wagnostic_get_audio_buffer(WagnosticContext* ctx);
uint8_t* wagnostic_get_wasm_memory(WagnosticContext* ctx, uint32_t* out_len);

/* Input manipulation */
void wagnostic_set_key(WagnosticContext* ctx, uint8_t scancode, uint8_t is_down);
void wagnostic_set_mouse(WagnosticContext* ctx, int32_t x, int32_t y, uint32_t buttons, int32_t wheel);
void wagnostic_set_gamepad(WagnosticContext* ctx, uint32_t buttons);
void wagnostic_set_ticks(WagnosticContext* ctx, uint32_t ticks_ms);

/* Render VRAM to raw 24-bit RGB (3 bytes/pixel) or 32-bit RGBA buffer */
int wagnostic_render_rgb24(WagnosticContext* ctx, uint8_t* out_rgb_buffer, size_t buffer_size);
int wagnostic_render_rgba32(WagnosticContext* ctx, uint8_t* out_rgba_buffer, size_t buffer_size);

/* GIF & Debug helpers */
int wagnostic_dump_ppm(WagnosticContext* ctx, const char* out_filename);
int wagnostic_record_gif(WagnosticContext* ctx, const char* out_filename, uint32_t total_frames, uint32_t frame_skip, uint16_t delay_cs);
void wagnostic_print_debug(WagnosticContext* ctx, FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* GIFNOSTIC_H */
