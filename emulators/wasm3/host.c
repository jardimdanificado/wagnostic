typedef struct { int x, y, w, h; } Rect;
/*
 * Wagnostic Reference Emulator — wasm3 + SDL2 host
 *
 * Loads a .wasm ROM file, executes its wupdate() function each frame.
 * wupdate() returns a pointer (i32 offset) to a WagnosticState struct
 * in WASM linear memory. The host reads/writes that struct directly.
 *
 * Build: see CMakeLists.txt
 * Usage: wagnostic <rom.wasm>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

static int32_t generate_unique_id(void) {
    uint64_t t = (uint64_t)time(NULL);
    uint64_t pc = (uint64_t)SDL_GetPerformanceCounter();
    uint32_t pid = 0;
#if defined(_WIN32)
    pid = (uint32_t)GetCurrentProcessId();
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    pid = (uint32_t)getpid();
#endif
    uint64_t h = t ^ (pc << 16) ^ ((uint64_t)pid << 32) ^ (uint64_t)clock();
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    int32_t res = (int32_t)h;
    return res ? res : 1;
}

/* ================================================================
 * WagnosticState struct (must match wagnostic.h exactly)
 * ================================================================ */



typedef struct {
    uint32_t width, height, scale;
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[676];
} WagnosticState;

static int g_is_tar = 0;
static char g_rom_path[1024] = {0};

/* ================================================================
 * TAR Helpers
 * ================================================================ */

static uint8_t* tar_extract_file(const char* tar_path, const char* target_filename, size_t* out_sz) {
    FILE* f = fopen(tar_path, "rb");
    if (!f) return NULL;
    uint8_t header[512];
    uint8_t* best_data = NULL;
    size_t best_sz = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') break;
        char name[101];
        memcpy(name, header, 100);
        name[100] = '\0';
        size_t size = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                size = size * 8 + (header[124+i] - '0');
        }
        if (strcmp(name, target_filename) == 0) {
            if (best_data) free(best_data);
            best_data = (uint8_t*)malloc(size);
            best_sz = size;
            fread(best_data, 1, size, f);
            long remainder = (512 - (size % 512)) % 512;
            fseek(f, remainder, SEEK_CUR);
        } else {
            long skip = size + ((512 - (size % 512)) % 512);
            fseek(f, skip, SEEK_CUR);
        }
    }
    fclose(f);
    if (out_sz) *out_sz = best_sz;
    return best_data;
}




static inline uint32_t compute_bpp(WagnosticState* s) {
    if (!s) return 32;
    uint32_t max_bit = 0;
    if (s->r_bits && s->r_shift + s->r_bits > max_bit) max_bit = s->r_shift + s->r_bits;
    if (s->g_bits && s->g_shift + s->g_bits > max_bit) max_bit = s->g_shift + s->g_bits;
    if (s->b_bits && s->b_shift + s->b_bits > max_bit) max_bit = s->b_shift + s->b_bits;
    if (s->a_bits && s->a_shift + s->a_bits > max_bit) max_bit = s->a_shift + s->a_bits;
    if (s->x_bits && s->x_shift + s->x_bits > max_bit) max_bit = s->x_shift + s->x_bits;
    
    // Round up to nearest standard size
    if (max_bit <= 1) return 1;
    if (max_bit <= 2) return 2;
    if (max_bit <= 4) return 4;
    if (max_bit <= 8) return 8;
    if (max_bit <= 16) return 16;
    if (max_bit <= 24) return 24;
    if (max_bit <= 32) return 32;
    return 64;
}

static_assert(sizeof(WagnosticState) == 1024, "WagnosticState size mismatch — check struct layout");

/* ================================================================
 * Globals
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

static uint8_t *g_mem     = NULL;
static uint32_t g_mem_len = 0;

static uint32_t g_state_ptr = 0;

/* ================================================================
 * Pointer helpers
 * ================================================================ */

static inline WagnosticState *get_state(void) {
    if (!g_mem || g_state_ptr == 0) return NULL;
    if (g_state_ptr + sizeof(WagnosticState) > g_mem_len) return NULL;
    return (WagnosticState *)(g_mem + g_state_ptr);
}

static inline uint8_t *get_vram(WagnosticState *s) {
    if (!s || s->vram_offset == 0) return NULL;
    return (uint8_t *)s + s->vram_offset;
}



/* ================================================================
 * Screen config with defaults
 * ================================================================ */

static void read_screen_config(WagnosticState *s,
                               uint32_t *W, uint32_t *H,
                               uint32_t *BPP, uint32_t *SCALE) {
    *W     = s ? s->width  : 0;
    *H     = s ? s->height : 0;
    *BPP = compute_bpp(s);
    *SCALE = s ? s->scale  : 0;
    if (*W == 0)     *W = 320;
    if (*H == 0)     *H = 240;
    if (*BPP == 0)   *BPP = 32;
    if (*SCALE == 0) *SCALE = 1;
}

/* ================================================================
 * Pixel conversion helpers
 * ================================================================ */

static inline uint32_t rgb332_to_abgr8888(uint8_t p) {
    uint32_t r = ((p >> 5) & 0x07) * 255 / 7;
    uint32_t g = ((p >> 2) & 0x07) * 255 / 7;
    uint32_t b = (p & 0x03) * 255 / 3;
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

static inline uint32_t rgb565_to_abgr8888(uint16_t p) {
    uint32_t r = ((p >> 11) & 0x1F) * 255 / 31;
    uint32_t g = ((p >> 5) & 0x3F) * 255 / 63;
    uint32_t b = (p & 0x1F) * 255 / 31;
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}



/* ================================================================
 * Render helpers
 * ================================================================ */

typedef enum {
    FMT_GENERIC = 0,
    FMT_RGBA8888_LE,
    FMT_BGRA8888_LE,
    FMT_RGBX8888_LE,
    FMT_BGRX8888_LE,
    FMT_RGB888,
    FMT_BGR888,
    FMT_RGB565,
    FMT_BGR565,
    FMT_RGB555,
    FMT_BGR555,
    FMT_RGB444,
    FMT_RGBA4444,
    FMT_ARGB4444,
    FMT_RGB333,
    FMT_RGB332,
    FMT_RGB222,
    FMT_RGBA2222,
    FMT_RGB111,
    FMT_GRAY8,
    FMT_RGB666,
    FMT_MONO1,
    FMT_MONO2,
    FMT_MONO4
} WagnosticPixelFormat;

static WagnosticPixelFormat detect_pixel_format(WagnosticState *s, uint32_t BPP,
                                               uint32_t *r_b_out, uint32_t *r_s_out,
                                               uint32_t *g_b_out, uint32_t *g_s_out,
                                               uint32_t *b_b_out, uint32_t *b_s_out,
                                               uint32_t *a_b_out, uint32_t *a_s_out) {
    uint32_t r_b = s ? s->r_bits : 0;
    uint32_t r_s = s ? s->r_shift : 0;
    uint32_t g_b = s ? s->g_bits : 0;
    uint32_t g_s = s ? s->g_shift : 0;
    uint32_t b_b = s ? s->b_bits : 0;
    uint32_t b_s = s ? s->b_shift : 0;
    uint32_t a_b = s ? s->a_bits : 0;
    uint32_t a_s = s ? s->a_shift : 0;
    uint32_t x_b = s ? s->x_bits : 0;

    if (!r_b && !g_b && !b_b && !a_b && !x_b) {
        if (BPP == 32) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; a_b = 8; a_s = 24; }
        else if (BPP == 24) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; }
        else if (BPP == 16) { r_b = 5; r_s = 11; g_b = 6; g_s = 5; b_b = 5; b_s = 0; }
        else if (BPP == 8)  { r_b = 3; r_s = 5;  g_b = 3; g_s = 2; b_b = 2; b_s = 0; }
        else if (BPP == 4 || BPP == 2 || BPP == 1) { a_b = BPP; a_s = 0; }
    }

    if (r_b_out) *r_b_out = r_b; if (r_s_out) *r_s_out = r_s;
    if (g_b_out) *g_b_out = g_b; if (g_s_out) *g_s_out = g_s;
    if (b_b_out) *b_b_out = b_b; if (b_s_out) *b_s_out = b_s;
    if (a_b_out) *a_b_out = a_b; if (a_s_out) *a_s_out = a_s;

    if (BPP == 1) return FMT_MONO1;
    if (BPP == 2) return FMT_MONO2;
    if (BPP == 4) return FMT_MONO4;

    if (BPP == 32) {
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16 && a_b == 8 && a_s == 24) return FMT_RGBA8888_LE;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0  && a_b == 8 && a_s == 24) return FMT_BGRA8888_LE;
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16 && a_b == 0) return FMT_RGBX8888_LE;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0  && a_b == 0) return FMT_BGRX8888_LE;
    }

    if (BPP == 24) {
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16) return FMT_RGB888;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0)  return FMT_BGR888;
    }

    if (BPP == 16) {
        if (r_b == 5 && r_s == 11 && g_b == 6 && g_s == 5  && b_b == 5 && b_s == 0  && a_b == 0) return FMT_RGB565;
        if (r_b == 5 && r_s == 0  && g_b == 6 && g_s == 5  && b_b == 5 && b_s == 11 && a_b == 0) return FMT_BGR565;
        if (r_b == 5 && r_s == 10 && g_b == 5 && g_s == 5  && b_b == 5 && b_s == 0  && (a_b == 0 || a_b == 1)) return FMT_RGB555;
        if (r_b == 5 && r_s == 0  && g_b == 5 && g_s == 5  && b_b == 5 && b_s == 10 && (a_b == 0 || a_b == 1)) return FMT_BGR555;
        if (r_b == 4 && r_s == 8  && g_b == 4 && g_s == 4  && b_b == 4 && b_s == 0  && a_b == 0) return FMT_RGB444;
        if (r_b == 4 && r_s == 12 && g_b == 4 && g_s == 8  && b_b == 4 && b_s == 4  && a_b == 4 && a_s == 0) return FMT_RGBA4444;
        if (r_b == 4 && r_s == 8  && g_b == 4 && g_s == 4  && b_b == 4 && b_s == 0  && a_b == 4 && a_s == 12) return FMT_ARGB4444;
        if (r_b == 3 && r_s == 6  && g_b == 3 && g_s == 3  && b_b == 3 && b_s == 0  && a_b == 0) return FMT_RGB333;
    }

    if (BPP == 8) {
        if (r_b == 3 && r_s == 5 && g_b == 3 && g_s == 2 && b_b == 2 && b_s == 0 && a_b == 0) return FMT_RGB332;
        if (r_b == 2 && r_s == 4 && g_b == 2 && g_s == 2 && b_b == 2 && b_s == 0 && a_b == 0) return FMT_RGB222;
        if (r_b == 2 && r_s == 6 && g_b == 2 && g_s == 4 && b_b == 2 && b_s == 2 && a_b == 2 && a_s == 0) return FMT_RGBA2222;
        if (r_b == 1 && r_s == 2 && g_b == 1 && g_s == 1 && b_b == 1 && b_s == 0 && a_b == 0) return FMT_RGB111;
        if (r_b == 0 && g_b == 0 && b_b == 0) return FMT_GRAY8;
    }

    if ((BPP == 24 || BPP == 32) && r_b == 6 && r_s == 12 && g_b == 6 && g_s == 6 && b_b == 6 && b_s == 0 && a_b == 0) {
        return FMT_RGB666;
    }

    return FMT_GENERIC;
}

static void render_rect_to_texture(SDL_Texture *texture, uint8_t *vram, WagnosticState *s,
                                   int rx, int ry, int rw, int rh,
                                   uint32_t W, uint32_t H, uint32_t BPP) {
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > (int)W) rw = (int)W - rx;
    if (ry + rh > (int)H) rh = (int)H - ry;
    if (rw <= 0 || rh <= 0) return;

    SDL_Rect sdl_rect = { rx, ry, rw, rh };
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, &sdl_rect, &pixels, &pitch);

    uint32_t r_b = 0, r_s = 0, g_b = 0, g_s = 0, b_b = 0, b_s = 0, a_b = 0, a_s = 0;
    WagnosticPixelFormat fmt = detect_pixel_format(s, BPP, &r_b, &r_s, &g_b, &g_s, &b_b, &b_s, &a_b, &a_s);

    switch (fmt) {
        case FMT_RGBA8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                memcpy(dst, src, rw * sizeof(uint32_t));
            }
            break;
        }
        case FMT_BGRA8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    dst[x] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
                }
            }
            break;
        }
        case FMT_RGBX8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    dst[x] = 0xFF000000 | (src[x] & 0x00FFFFFF);
                }
            }
            break;
        }
        case FMT_BGRX8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    dst[x] = 0xFF000000 | (px & 0x0000FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
                }
            }
            break;
        }
        case FMT_RGB888: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + (y * W + rx) * 3;
                for (int x = 0; x < rw; x++) {
                    dst[x] = 0xFF000000 | (src[2] << 16) | (src[1] << 8) | src[0];
                    src += 3;
                }
            }
            break;
        }
        case FMT_BGR888: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + (y * W + rx) * 3;
                for (int x = 0; x < rw; x++) {
                    dst[x] = 0xFF000000 | (src[0] << 16) | (src[1] << 8) | src[2];
                    src += 3;
                }
            }
            break;
        }
        case FMT_RGB565: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t b = px & 0x1F;        b = (b << 3) | (b >> 2);
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_BGR565: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t b = (px >> 11) & 0x1F; b = (b << 3) | (b >> 2);
                    uint32_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t r = px & 0x1F;        r = (r << 3) | (r >> 2);
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB555: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 10) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (px >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
                    uint32_t b = px & 0x1F;        b = (b << 3) | (b >> 2);
                    uint32_t a = (a_b == 1 && !(px & (1 << a_s))) ? 0 : 255;
                    dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_BGR555: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t b = (px >> 10) & 0x1F; b = (b << 3) | (b >> 2);
                    uint32_t g = (px >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
                    uint32_t r = px & 0x1F;        r = (r << 3) | (r >> 2);
                    uint32_t a = (a_b == 1 && !(px & (1 << a_s))) ? 0 : 255;
                    dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 8) & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 4) & 0x0F; g = (g << 4) | g;
                    uint32_t b = px & 0x0F;        b = (b << 4) | b;
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGBA4444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 12) & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 8)  & 0x0F; g = (g << 4) | g;
                    uint32_t b = (px >> 4)  & 0x0F; b = (b << 4) | b;
                    uint32_t a = px & 0x0F;         a = (a << 4) | a;
                    dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_ARGB4444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t a = (px >> 12) & 0x0F; a = (a << 4) | a;
                    uint32_t r = (px >> 8)  & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 4)  & 0x0F; g = (g << 4) | g;
                    uint32_t b = px & 0x0F;         b = (b << 4) | b;
                    dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB333: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 6) & 7; r = (r << 5) | (r << 2) | (r >> 1);
                    uint32_t g = (px >> 3) & 7; g = (g << 5) | (g << 2) | (g >> 1);
                    uint32_t b = px & 7;        b = (b << 5) | (b << 2) | (b >> 1);
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB332: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 5) & 7; r = (r << 5) | (r << 2) | (r >> 1);
                    uint32_t g = (px >> 2) & 7; g = (g << 5) | (g << 2) | (g >> 1);
                    uint32_t b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB222: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 4) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
                    uint32_t g = (px >> 2) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
                    uint32_t b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGBA2222: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 6) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
                    uint32_t g = (px >> 4) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
                    uint32_t b = (px >> 2) & 3; b = (b << 6) | (b << 4) | (b << 2) | b;
                    uint32_t a = px & 3;        a = (a << 6) | (a << 4) | (a << 2) | a;
                    dst[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB111: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px & 4) ? 255 : 0;
                    uint32_t g = (px & 2) ? 255 : 0;
                    uint32_t b = (px & 1) ? 255 : 0;
                    dst[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_GRAY8: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t lum = src[x];
                    dst[x] = 0xFF000000 | (lum << 16) | (lum << 8) | lum;
                }
            }
            break;
        }
        case FMT_RGB666: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint32_t px = (BPP == 32) ? ((uint32_t*)vram)[idx] : (vram[idx*3] | (vram[idx*3+1]<<8) | (vram[idx*3+2]<<16));
                    uint32_t r = (px >> 12) & 0x3F; r = (r << 2) | (r >> 4);
                    uint32_t g = (px >> 6)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t b = px & 0x3F;         b = (b << 2) | (b >> 4);
                    dst[x - rx] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_MONO1: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 8];
                    uint8_t bit = (b_val >> (7 - (idx % 8))) & 1;
                    dst[x - rx] = bit ? 0xFFFFFFFF : 0xFF000000;
                }
            }
            break;
        }
        case FMT_MONO2: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 4];
                    uint8_t val = (b_val >> (6 - (idx % 4) * 2)) & 0x03;
                    val = (val << 6) | (val << 4) | (val << 2) | val;
                    dst[x - rx] = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
            break;
        }
        case FMT_MONO4: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 2];
                    uint8_t val = (idx % 2 == 0) ? (b_val >> 4) : (b_val & 0x0F);
                    val = (val << 4) | val;
                    dst[x - rx] = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
            break;
        }
        default: {
            int is_grayscale = (!r_b && !g_b && !b_b && a_b);
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
                for (int x = rx; x < rx + rw; x++) {
                    uint64_t px = 0;
                    size_t idx = y * W + x;
                    if (BPP == 64) px = ((uint64_t*)vram)[idx];
                    else if (BPP == 32) px = ((uint32_t*)vram)[idx];
                    else if (BPP == 24) {
                        uint8_t *p = vram + idx * 3;
                        px = p[0] | (p[1] << 8) | (p[2] << 16);
                    }
                    else if (BPP == 16) px = ((uint16_t*)vram)[idx];
                    else if (BPP == 8) px = vram[idx];
                    else if (BPP == 4) {
                        uint8_t b = vram[idx / 2];
                        px = (idx % 2 == 0) ? (b >> 4) : (b & 0x0F);
                    }
                    else if (BPP == 2) {
                        uint8_t b = vram[idx / 4];
                        px = (b >> (6 - (idx % 4) * 2)) & 0x03;
                    }
                    else if (BPP == 1) {
                        uint8_t b = vram[idx / 8];
                        px = (b >> (7 - (idx % 8))) & 1;
                    }

                    uint32_t r = 0, g = 0, b = 0, a = 255;
                    if (is_grayscale) {
                        uint32_t lum = (uint32_t)(((px >> a_s) & ((1ULL << a_b) - 1)) * 255 / ((1ULL << a_b) - 1));
                        r = g = b = lum;
                        a = 255;
                    } else {
                        if (r_b) r = (uint32_t)(((px >> r_s) & ((1ULL << r_b) - 1)) * 255 / ((1ULL << r_b) - 1));
                        if (g_b) g = (uint32_t)(((px >> g_s) & ((1ULL << g_b) - 1)) * 255 / ((1ULL << g_b) - 1));
                        if (b_b) b = (uint32_t)(((px >> b_s) & ((1ULL << b_b) - 1)) * 255 / ((1ULL << b_b) - 1));
                        if (a_b) a = (uint32_t)(((px >> a_s) & ((1ULL << a_b) - 1)) * 255 / ((1ULL << a_b) - 1));
                    }
                    dst[x - rx] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
    }

    SDL_UnlockTexture(texture);
}

static void render_fullscreen(SDL_Texture *texture, uint8_t *vram, WagnosticState *s,
                              uint32_t W, uint32_t H, uint32_t BPP) {
    render_rect_to_texture(texture, vram, s, 0, 0, W, H, W, H, BPP);
}

/* ================================================================
 * Aspect-ratio-correct letterbox
 * ================================================================ */

static void calc_letterbox(int win_w, int win_h, uint32_t W, uint32_t H,
                           SDL_Rect *dst) {
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    if (aspect_win > aspect_rom) {
        dst->h = win_h;
        dst->w = (int)(win_h * aspect_rom);
        dst->x = (win_w - dst->w) / 2;
        dst->y = 0;
    } else {
        dst->w = win_w;
        dst->h = (int)(win_w / aspect_rom);
        dst->x = 0;
        dst->y = (win_h - dst->h) / 2;
    }
}

/* ================================================================
 * Mouse coordinate conversion
 * ================================================================ */

static void convert_mouse_coords(int wx, int wy, int *rx, int *ry,
                                 int win_w, int win_h, uint32_t W, uint32_t H) {
    SDL_Rect dst;
    calc_letterbox(win_w, win_h, W, H, &dst);
    float scale_x = (float)W / (float)dst.w;
    float scale_y = (float)H / (float)dst.h;
    *rx = (int)((wx - dst.x) * scale_x);
    *ry = (int)((wy - dst.y) * scale_y);
    if (*rx < 0) *rx = 0;
    if (*rx >= (int)W) *rx = (int)W - 1;
    if (*ry < 0) *ry = 0;
    if (*ry >= (int)H) *ry = (int)H - 1;
}
/* ================================================================
 * Refresh WASM memory pointer
 * ================================================================ */

static void refresh_memory(void) {
    g_mem = m3_GetMemory(g_runtime, &g_mem_len, 0);
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    strncpy(g_rom_path, argv[1], sizeof(g_rom_path)-1);

    /* ---- Load ROM (TAR or Raw WASM) ---- */
    uint8_t *wasm_data = NULL;
    size_t sz = 0;
    
    wasm_data = tar_extract_file(g_rom_path, "main.wasm", &sz);
    if (wasm_data) {
        g_is_tar = 1;
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("Failed to open ROM"); return 1; }
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        wasm_data = (uint8_t *)malloc(sz);
        if (!wasm_data) { fprintf(stderr, "Out of memory\n"); fclose(f); return 1; }
        fread(wasm_data, 1, sz, f);
        fclose(f);
    }

    /* ---- Initialize wasm3 ---- */
    IM3Environment env = m3_NewEnvironment();
    if (!env) { fprintf(stderr, "m3_NewEnvironment failed\n"); free(wasm_data); return 1; }

    g_runtime = m3_NewRuntime(env, 64 * 1024 * 1024, NULL);
    if (!g_runtime) { fprintf(stderr, "m3_NewRuntime failed\n"); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    M3Result result = m3_ParseModule(env, &g_module, wasm_data, sz);
    if (result) { fprintf(stderr, "Parse error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    result = m3_LoadModule(g_runtime, g_module);
    if (result) { fprintf(stderr, "Load error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    /* ---- Find wupdate ---- */
    IM3Function f_wupdate = NULL;
    result = m3_FindFunction(&f_wupdate, g_runtime, "wupdate");
    if (result || !f_wupdate) {
        fprintf(stderr, "ROM does not export wupdate(): %s\n", result ? result : "not found");
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Refresh WASM memory ---- */
    refresh_memory();

    /* ---- Initialize SDL ---- */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Call wupdate() once to get initial state pointer ---- */
    {
        m3_CallV(f_wupdate);
        m3_GetResultsV(f_wupdate, &g_state_ptr);
    }
    refresh_memory();

    int32_t session_unique = generate_unique_id();
    WagnosticState *state = get_state();
    if (state) {
        state->unique = session_unique;
    }

    /* ---- Read initial config ---- */
    uint32_t W, H, BPP, SCALE;
    read_screen_config(state, &W, &H, &BPP, &SCALE);

    /* ---- Create window ---- */
    SDL_Window *window = SDL_CreateWindow(
        "Wagnostic",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(W * SCALE), (int)(H * SCALE),
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit(); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit();
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
        (int)W, (int)H);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    /* ---- Track previous config for change detection ---- */
    uint32_t prev_W = W, prev_H = H, prev_BPP = BPP, prev_SCALE = SCALE;
    uint8_t keys_state[256];
    memset(keys_state, 0, sizeof(keys_state));

    uint32_t mouse_buttons = 0;
    int mouse_x = 0, mouse_y = 0;
    int mouse_wheel = 0;
    uint32_t gamepad_buttons = 0;

    /* ================================================================
     * Main loop
     * ================================================================ */

    int running = 1;
    while (running) {

        /* ---- Poll SDL events ---- */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (ev.key.keysym.scancode < 256) {
                    keys_state[ev.key.keysym.scancode] =
                        (ev.type == SDL_KEYDOWN) ? 1 : 0;
                }
                break;

            case SDL_MOUSEMOTION: {
                int win_w, win_h;
                SDL_GetWindowSize(window, &win_w, &win_h);
                convert_mouse_coords(ev.motion.x, ev.motion.y,
                                     &mouse_x, &mouse_y,
                                     win_w, win_h, W, H);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int pressed = (ev.type == SDL_MOUSEBUTTONDOWN);
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    if (pressed) mouse_buttons |= 1; else mouse_buttons &= ~1u;
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    if (pressed) mouse_buttons |= 2; else mouse_buttons &= ~2u;
                }
                break;
            }

            case SDL_MOUSEWHEEL:
                mouse_wheel += ev.wheel.y;
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                int pressed = (ev.type == SDL_CONTROLLERBUTTONDOWN);
                SDL_GameControllerButton btn = ev.cbutton.button;
                uint32_t mask = 0;
                switch (btn) {
                    case SDL_CONTROLLER_BUTTON_A:      mask = 0x0001; break;
                    case SDL_CONTROLLER_BUTTON_B:      mask = 0x0002; break;
                    case SDL_CONTROLLER_BUTTON_X:      mask = 0x0004; break;
                    case SDL_CONTROLLER_BUTTON_Y:      mask = 0x0008; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  mask = 0x0010; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: mask = 0x0020; break;
                    case SDL_CONTROLLER_BUTTON_BACK:   mask = 0x0040; break;
                    case SDL_CONTROLLER_BUTTON_START:  mask = 0x0080; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:  mask = 0x0100; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: mask = 0x0200; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    mask = 0x0400; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  mask = 0x0800; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  mask = 0x1000; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: mask = 0x2000; break;
                    default: break;
                }
                if (mask) {
                    if (pressed) gamepad_buttons |= mask;
                    else         gamepad_buttons &= ~mask;
                }
                break;
            }

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {}
                break;
            }
        }

        /* ---- Step 1: Write input to state struct ---- */
        state = get_state();
        if (state) {
            memcpy(state->keys, keys_state, 256);
            state->mouse_x = mouse_x;
            state->mouse_y = mouse_y;
            state->mouse_buttons = mouse_buttons;
            state->mouse_wheel = mouse_wheel;
            state->gamepad_buttons = gamepad_buttons;
            state->ticks = SDL_GetTicks();
            if (state->unique == 0) state->unique = session_unique;
        }

        /* ---- Step 2: Call wupdate(), exit if return is 0 ---- */
        {
            int32_t keep = 0;
            m3_CallV(f_wupdate);
            m3_GetResultsV(f_wupdate, &keep);
            if (!keep) break;
            g_state_ptr = (uint32_t)keep;
        }

        /* ---- Refresh memory pointer (ROM may have grown it) ---- */
        refresh_memory();
        state = get_state();



        /* ---- Step 3: Read config and detect changes ---- */
        read_screen_config(state, &W, &H, &BPP, &SCALE);

        int config_changed = (W != prev_W || H != prev_H ||
                              BPP != prev_BPP || SCALE != prev_SCALE);

        if (config_changed) {
            SDL_SetWindowSize(window, (int)(W * SCALE), (int)(H * SCALE));
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                (int)W, (int)H);
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            prev_W = W; prev_H = H; prev_BPP = BPP; prev_SCALE = SCALE;

            int win_w, win_h;
            SDL_GetWindowSize(window, &win_w, &win_h);
            convert_mouse_coords(mouse_x, mouse_y, &mouse_x, &mouse_y,
                                 win_w, win_h, W, H);
        }

        /* ---- Step 4: Render dirty rects ---- */
        if (state) {
            uint8_t *vram = get_vram(state);

            uint32_t dirty_count = 0;
            int32_t* rects = NULL;
            if (state->dirty_rects) {
                dirty_count = *(uint32_t*)(g_mem + state->dirty_rects);
                rects = (int32_t*)(g_mem + state->dirty_rects + 4);
            }

            if (vram && dirty_count > 0) {
                if (dirty_count == 1) {
                    int rx = rects[0];
                    int ry = rects[1];
                    int rw = rects[2];
                    int rh = rects[3];
                    if (rx == 0 && ry == 0 &&
                        (uint32_t)rw == W && (uint32_t)rh == H) {
                        render_fullscreen(texture, vram, state, W, H, BPP);
                    } else {
                        render_rect_to_texture(texture, vram, state,
                            rx, ry, rw, rh, W, H, BPP);
                    }
                } else {
                    uint32_t count = dirty_count;
                    if (count > 32) count = 32;
                    for (uint32_t i = 0; i < count; i++) {
                        int rx = rects[i*4];
                        int ry = rects[i*4+1];
                        int rw = rects[i*4+2];
                        int rh = rects[i*4+3];
                        render_rect_to_texture(texture, vram, state,
                            rx, ry, rw, rh, W, H, BPP);
                    }
                }

                int win_w, win_h;
                SDL_GetWindowSize(window, &win_w, &win_h);
                SDL_Rect dst;
                calc_letterbox(win_w, win_h, W, H, &dst);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &dst);
                SDL_RenderPresent(renderer);

                state->dirty_rects = 0;
            }
        }

        /* ---- Step 5: Reset mouse wheel ---- */
        if (state) state->mouse_wheel = 0;
        mouse_wheel = 0;

        /* ---- Yield / FPS limit ---- */
        if (state && state->target_fps > 0) {
            static uint32_t frame_start = 0;
            uint32_t now = SDL_GetTicks();
            uint32_t elapsed = now - frame_start;
            int32_t delay = (1000 / state->target_fps) - (int32_t)elapsed;
            if (delay > 0) SDL_Delay((uint32_t)delay);
            frame_start = SDL_GetTicks();
        } else {
            SDL_Delay(1);
        }
    }

    /* ================================================================
     * Cleanup
     * ================================================================ */

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
