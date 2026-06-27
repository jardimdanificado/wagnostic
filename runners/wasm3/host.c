/*
 * Wagnostic Native Runner — wasm3 + SDL2 host
 *
 * Loads a .wasm ROM file, executes its wupdate() function each frame,
 * and provides video/audio/input services via WASM global ABI.
 *
 * ROMs export i32 global pointers into WASM linear memory.
 * The host reads/writes the pointed-to values each frame.
 *
 * Build: see CMakeLists.txt
 * Usage: wagnostic <rom.wasm>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

/* ================================================================
 * wasm3 strlen — imported by ROMs via (import "env" "strlen")
 * ================================================================ */

m3ApiRawFunction(m3_libc_strlen) {
    m3ApiReturnType(int32_t)
    m3ApiGetArgMem(const char *, str)
    int32_t len = 0;
    if (str) {
        while (str[len] != '\0')
            len++;
    }
    m3ApiReturn(len);
}

/* ================================================================
 * Math primitives — backed by glibc. WASM lowering of sin/cos/exp/
 * log/pow emits `env.*` imports, resolved here at link time.
 * ================================================================ */
#include <math.h>

m3ApiRawFunction(m3_env_sin)   { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(sin(x)); }
m3ApiRawFunction(m3_env_cos)   { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(cos(x)); }
m3ApiRawFunction(m3_env_exp)   { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(exp(x)); }
m3ApiRawFunction(m3_env_log)   { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(log(x)); }
m3ApiRawFunction(m3_env_pow)   { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiGetArg(double, y); m3ApiReturn(pow(x, y)); }
m3ApiRawFunction(m3_env_ldexp) { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiGetArg(int32_t, n); m3ApiReturn(ldexp(x, n)); }
m3ApiRawFunction(m3_env_fabs)  { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(fabs(x)); }
m3ApiRawFunction(m3_env_floor) { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(floor(x)); }
m3ApiRawFunction(m3_env_ceil)  { m3ApiReturnType(double); m3ApiGetArg(double, x); m3ApiReturn(ceil(x)); }

/* ================================================================
 * Globals
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

/* Cached pointers into WASM linear memory (resolved once per frame) */
static uint8_t *g_mem          = NULL;
static uint32_t g_mem_len      = 0;

/* Pointer offsets for frequently-used globals */
static IM3Global g_g_w_vram         = NULL;
static IM3Global g_g_w_dirty_count  = NULL;
static IM3Global g_g_w_dirty_rects  = NULL;
static IM3Global g_g_w_keys         = NULL;
static IM3Global g_g_w_mouse_x      = NULL;
static IM3Global g_g_w_mouse_y      = NULL;
static IM3Global g_g_w_mouse_buttons= NULL;
static IM3Global g_g_w_mouse_wheel  = NULL;
static IM3Global g_g_w_target_fps   = NULL;
static IM3Global g_g_w_gamepad_buttons = NULL;
static IM3Global g_g_w_ticks        = NULL;
static IM3Global g_g_w_width        = NULL;
static IM3Global g_g_w_height       = NULL;
static IM3Global g_g_w_bpp          = NULL;
static IM3Global g_g_w_scale        = NULL;
static IM3Global g_g_w_title        = NULL;
static IM3Global g_g_w_audio_size   = NULL;
static IM3Global g_g_w_audio_sample_rate = NULL;
static IM3Global g_g_w_audio_bpp    = NULL;
static IM3Global g_g_w_audio_channels = NULL;
static IM3Global g_g_w_audio_write  = NULL;
static IM3Global g_g_w_audio_read   = NULL;
static IM3Global g_g_w_audio_buffer = NULL;
static IM3Global g_g_w_audio_underrun = NULL;
static IM3Global g_g_w_audio_overrun  = NULL;

/* ================================================================
 * Pixel lookup tables (LUTs) for fast format conversion
 *
 * Pre-computed RGB332→ABGR8888 and RGB565→ABGR8888 mappings.
 * Avoids per-pixel division: 1 memory lookup vs ~10 arithmetic ops.
 *
 * Memory cost: 1KB (8-bit) + 256KB (16-bit) = 257KB total.
 * ================================================================ */

static uint32_t rgb332_lut[256];      /* 1KB  — 8bpp RGB332 */
static uint32_t rgb565_lut[65536];    /* 256KB — 16bpp RGB565 */
static int pixel_lut_initialized = 0;

/* Audio buffer lock. The host runs wupdate() in the main thread (which
 * writes samples into the shared w_audio_buffer via the ROM's fill_audio)
 * and host_audio_callback() in SDL's audio thread (which reads from the
 * same buffer). On multi-core systems the two threads can run truly
 * concurrently, and the audio thread can read a position the main thread
 * is mid-write — that produces a torn sample, heard as a pop.
 *
 * We serialize the two sides with a single mutex. The main thread
 * acquires it around m3_CallV(f_wupdate) and the audio thread holds
 * it for the whole callback. Writes are small (<2KB) and complete in
 * microseconds, so the audio thread is only ever blocked briefly.
 */
static SDL_mutex *g_audio_mutex = NULL;

/* ================================================================
 * Pointer resolution helpers
 * ================================================================ */

static uint32_t resolve_i32(IM3Global g) {
    if (!g) return 0;
    M3TaggedValue v;
    if (m3_GetGlobal(g, &v)) return 0;
    return (uint32_t)v.value.i32;
}

/* Resolve the i32 global and return a pointer into linear memory.
 * Returns NULL if the global or memory is unavailable. */
static void *resolve_ptr(IM3Global g) {
    if (!g) return NULL;
    M3TaggedValue v;
    if (m3_GetGlobal(g, &v)) return NULL;
    uint32_t off = (uint32_t)v.value.i32;
    if (off == 0) return NULL;
    if (!g_mem) return NULL;
    return g_mem + off;
}

/* ================================================================
 * Cached pointer-to-data helpers (avoid repeated resolve_i32)
 * ================================================================ */

static uint32_t *ptr_u32(IM3Global g) {
    return (uint32_t *)resolve_ptr(g);
}

static int32_t *ptr_i32(IM3Global g) {
    return (int32_t *)resolve_ptr(g);
}

static uint8_t *ptr_u8(IM3Global g) {
    return (uint8_t *)resolve_ptr(g);
}

/* ================================================================
 * High-level read/write helpers using cached globals
 * ================================================================ */

static uint32_t read_u32(IM3Global g) {
    uint32_t *p = ptr_u32(g);
    return p ? *p : 0;
}

static int32_t read_i32(IM3Global g) {
    int32_t *p = ptr_i32(g);
    return p ? *p : 0;
}

static void write_u32(IM3Global g, uint32_t val) {
    uint32_t *p = ptr_u32(g);
    if (p) *p = val;
}

static void write_i32(IM3Global g, int32_t val) {
    int32_t *p = ptr_i32(g);
    if (p) *p = val;
}

static void read_str(IM3Global g, char *dst, int max) {
    if (!g || !dst || max <= 0) { if (dst) dst[0] = '\0'; return; }
    M3TaggedValue v;
    if (m3_GetGlobal(g, &v)) { dst[0] = '\0'; return; }
    uint32_t off = (uint32_t)v.value.i32;
    if (off == 0 || !g_mem) { dst[0] = '\0'; return; }
    const char *src = (const char *)(g_mem + off);
    strncpy(dst, src, max - 1);
    dst[max - 1] = '\0';
}

/* ================================================================
 * Dirty rect reading
 * ================================================================ */

/* Each dirty rect is 4 x i32 = 16 bytes: {x, y, w, h} */
static void read_dirty_rect(int idx, int *rx, int *ry, int *rw, int *rh) {
    uint8_t *base = ptr_u8(g_g_w_dirty_rects);
    if (!base) { *rx = *ry = *rw = *rh = 0; return; }
    int32_t *r = (int32_t *)(base + idx * 16);
    *rx = r[0]; *ry = r[1]; *rw = r[2]; *rh = r[3];
}

/* ================================================================
 * Cache all global pointers from the module
 * ================================================================ */

static void cache_globals(void) {
    g_g_w_vram          = m3_FindGlobal(g_module, "w_vram");
    g_g_w_dirty_count   = m3_FindGlobal(g_module, "w_dirty_count");
    g_g_w_dirty_rects   = m3_FindGlobal(g_module, "w_dirty_rects");
    g_g_w_keys          = m3_FindGlobal(g_module, "w_keys");
    g_g_w_mouse_x       = m3_FindGlobal(g_module, "w_mouse_x");
    g_g_w_mouse_y       = m3_FindGlobal(g_module, "w_mouse_y");
    g_g_w_mouse_buttons = m3_FindGlobal(g_module, "w_mouse_buttons");
    g_g_w_mouse_wheel   = m3_FindGlobal(g_module, "w_mouse_wheel");
    g_g_w_target_fps    = m3_FindGlobal(g_module, "w_target_fps");
    g_g_w_gamepad_buttons = m3_FindGlobal(g_module, "w_gamepad_buttons");
    g_g_w_ticks         = m3_FindGlobal(g_module, "w_ticks");
    g_g_w_width         = m3_FindGlobal(g_module, "w_width");
    g_g_w_height        = m3_FindGlobal(g_module, "w_height");
    g_g_w_bpp           = m3_FindGlobal(g_module, "w_bpp");
    g_g_w_scale         = m3_FindGlobal(g_module, "w_scale");
    g_g_w_title         = m3_FindGlobal(g_module, "w_title");
    g_g_w_audio_size       = m3_FindGlobal(g_module, "w_audio_size");
    g_g_w_audio_sample_rate= m3_FindGlobal(g_module, "w_audio_sample_rate");
    g_g_w_audio_bpp        = m3_FindGlobal(g_module, "w_audio_bpp");
    g_g_w_audio_channels   = m3_FindGlobal(g_module, "w_audio_channels");
    g_g_w_audio_write      = m3_FindGlobal(g_module, "w_audio_write");
    g_g_w_audio_read       = m3_FindGlobal(g_module, "w_audio_read");
    g_g_w_audio_buffer     = m3_FindGlobal(g_module, "w_audio_buffer");
    g_g_w_audio_underrun   = m3_FindGlobal(g_module, "w_audio_underrun");
    g_g_w_audio_overrun    = m3_FindGlobal(g_module, "w_audio_overrun");
}

/* ================================================================
 * Audio callback (runs on SDL audio thread)
 *
 * RULES FOR ROM AUTHORS (prevents race conditions):
 *   1. Write samples into w_audio_buffer FIRST
 *   2. THEN update w_audio_write ONCE, at the very end
 *   3. NEVER read w_audio_write while writing samples
 *   4. NEVER read w_audio_read (host owns it)
 *   5. Use the provided fill_audio_buffer() helper — it follows all rules
 *
 * RULES FOR HOST AUTHORS:
 *   1. Read w_audio_write ONCE at the start of the callback
 *   2. Process all samples
 *   3. Write w_audio_read ONCE at the very end
 *   4. NEVER write to w_audio_write (ROM owns it)
 *   5. NEVER read w_audio_read from another thread
 * ================================================================ */

static void host_audio_callback(void *userdata, Uint8 *stream_ptr, int len_bytes) {
    (void)userdata;
    float *stream = (float *)stream_ptr;
    int nsamples  = len_bytes / (int)sizeof(float);

    /* Block the main thread from writing the audio buffer while we read
     * it. See g_audio_mutex declaration for the full rationale. */
    if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);

    /* ---- Step 1: Read pointers ONCE (snapshot) ---- */
    uint32_t size  = read_u32(g_g_w_audio_size);
    uint32_t bpp   = read_u32(g_g_w_audio_bpp);
    uint32_t r_off = read_u32(g_g_w_audio_read);
    uint32_t w_off = read_u32(g_g_w_audio_write);

    uint8_t *abuf = ptr_u8(g_g_w_audio_buffer);

    /* ---- Step 2: Validate everything ---- */
    if (!abuf || size == 0 || bpp == 0 || bpp > 4) {
        memset(stream, 0, len_bytes);
        return;
    }

    /* Clamp pointers to valid range */
    if (r_off >= size) r_off = 0;
    if (w_off >= size) w_off = 0;

    /* ---- Step 3: Calculate available bytes ---- */
    /* Buffer uses size-1 usable slots: when r==w it means EMPTY.
     * When (w+1)%size==r it means FULL. This avoids the ambiguity
     * of r==w meaning both empty and full. */
    uint32_t avail;
    if (w_off >= r_off)
        avail = w_off - r_off;
    else
        avail = size - r_off + w_off;

    /* ---- Step 4: Read samples, handle wrap-around ----
     *
     * max_bytes must be capped at what the audio actually consumes
     * (nsamples * bpp), NOT at the f32 output size (nsamples * 4).
     * Otherwise r_off advances by the f32-equivalent bytes while only
     * bpp bytes per sample are read, and the read pointer drifts past
     * the audio's true position in the buffer. After a few callbacks
     * the audio is reading samples from a half-period away in the
     * looping pcm — heard as a 180° phase flip / pop at every callback
     * boundary. */
    uint32_t bytes_per_sample = bpp;
    uint32_t max_bytes = avail;
    uint32_t stream_bytes = (uint32_t)nsamples * bpp;
    if (max_bytes > stream_bytes) max_bytes = stream_bytes;

    /* Round down to whole samples */
    max_bytes = (max_bytes / bytes_per_sample) * bytes_per_sample;

    uint32_t bytes_read = 0;
    int stream_idx = 0;

    while (bytes_read < max_bytes && stream_idx < nsamples) {
        uint32_t chunk = max_bytes - bytes_read;
        uint32_t pos = (r_off + bytes_read) % size;

        /* Clamp chunk to end of buffer (wrap-around boundary) */
        uint32_t to_end = size - pos;
        if (chunk > to_end) chunk = to_end;

        /* Convert samples in this chunk */
        if (bpp == 1) {
            /* u8: unsigned 8-bit, centre at 128 */
            for (uint32_t b = 0; b < chunk && stream_idx < nsamples; b++) {
                stream[stream_idx++] = ((float)abuf[pos + b] - 128.0f) / 128.0f;
            }
        } else if (bpp == 2) {
            /* s16 little-endian */
            for (uint32_t b = 0; b + 1 < chunk && stream_idx < nsamples; b += 2) {
                int16_t s16;
                memcpy(&s16, abuf + pos + b, 2);
                stream[stream_idx++] = (float)s16 / 32768.0f;
            }
        } else {
            /* bpp == 4: f32 */
            for (uint32_t b = 0; b + 3 < chunk && stream_idx < nsamples; b += 4) {
                float sample;
                memcpy(&sample, abuf + pos + b, 4);
                stream[stream_idx++] = sample;
            }
        }

        bytes_read += chunk;
    }

    /* ---- Step 5: Fill remaining with silence ---- */
    int underrun = (stream_idx < nsamples);
    for (int i = stream_idx; i < nsamples; i++) {
        stream[i] = 0.0f;
    }

    /* ---- Step 6: Write read pointer ONCE ---- */
    if (bytes_read > 0) {
        write_u32(g_g_w_audio_read, (r_off + bytes_read) % size);
    }

    /* ---- Step 7: Update underrun counter ---- */
    if (underrun) {
        uint32_t ur = read_u32(g_g_w_audio_underrun);
        write_u32(g_g_w_audio_underrun, ur + 1);
    }

    /* Release the audio buffer lock so the main thread can write again. */
    if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
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

/* 32bpp RGBA8888 is already in ABGR8888 memory layout — direct copy */

static void init_pixel_luts(void) {
    if (pixel_lut_initialized) return;

    for (int i = 0; i < 256; i++) {
        uint32_t r = ((i >> 5) & 0x07) * 36;
        uint32_t g = ((i >> 2) & 0x07) * 36;
        uint32_t b = (i & 0x03) * 85;
        rgb332_lut[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }

    for (int i = 0; i < 65536; i++) {
        uint32_t r = ((i >> 11) & 0x1F) * 255 / 31;
        uint32_t g = ((i >> 5) & 0x3F) * 255 / 63;
        uint32_t b = (i & 0x1F) * 255 / 31;
        rgb565_lut[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }

    pixel_lut_initialized = 1;
}

/* ================================================================
 * Render a rect from VRAM to the SDL texture
 * ================================================================ */

static void render_rect_to_texture(SDL_Texture *texture, uint8_t *vram,
                                   int rx, int ry, int rw, int rh,
                                   uint32_t W, uint32_t H, uint32_t BPP) {
    /* Clamp rect to screen bounds */
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > (int)W) rw = (int)W - rx;
    if (ry + rh > (int)H) rh = (int)H - ry;
    if (rw <= 0 || rh <= 0) return;

    SDL_Rect sdl_rect = { rx, ry, rw, rh };
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, &sdl_rect, &pixels, &pitch);

    if (BPP == 8) {
        for (int y = ry; y < ry + rh; y++) {
            uint8_t  *src = vram + y * W + rx;
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            for (int x = 0; x < rw; x++) {
                dst[x] = rgb332_lut[src[x]];
            }
        }
    } else if (BPP == 16) {
        for (int y = ry; y < ry + rh; y++) {
            uint16_t *src = (uint16_t *)(vram + (y * W + rx) * 2);
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            for (int x = 0; x < rw; x++) {
                dst[x] = rgb565_lut[src[x]];
            }
        }
    } else if (BPP == 32) {
        for (int y = ry; y < ry + rh; y++) {
            uint8_t  *src = vram + (y * W + rx) * 4;
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            memcpy(dst, src, rw * 4);
        }
    }

    SDL_UnlockTexture(texture);
}

/* ================================================================
 * Full-screen bulk copy (fast path)
 * ================================================================ */

static void render_fullscreen(SDL_Texture *texture, uint8_t *vram,
                              uint32_t W, uint32_t H, uint32_t BPP) {
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    if (BPP == 8) {
        uint8_t  *src = vram;
        uint32_t *dst = (uint32_t *)pixels;
        uint32_t total = W * H;
        for (uint32_t i = 0; i < total; i++) {
            dst[i] = rgb332_lut[src[i]];
        }
    } else if (BPP == 16) {
        uint16_t *src = (uint16_t *)vram;
        uint32_t *dst = (uint32_t *)pixels;
        uint32_t total = W * H;
        for (uint32_t i = 0; i < total; i++) {
            dst[i] = rgb565_lut[src[i]];
        }
    } else if (BPP == 32) {
        /* Direct memcpy — 32bpp ROM format matches ABGR8888 layout */
        memcpy(pixels, vram, W * H * 4);
    }

    SDL_UnlockTexture(texture);
}

/* ================================================================
 * Aspect-ratio-correct letterbox calculation
 * ================================================================ */

static void calc_letterbox(int win_w, int win_h, uint32_t W, uint32_t H,
                           SDL_Rect *dst) {
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;

    if (aspect_win > aspect_rom) {
        /* Window is wider than ROM — pillarbox */
        dst->h = win_h;
        dst->w = (int)(win_h * aspect_rom);
        dst->x = (win_w - dst->w) / 2;
        dst->y = 0;
    } else {
        /* Window is taller than ROM — letterbox */
        dst->w = win_w;
        dst->h = (int)(win_w / aspect_rom);
        dst->x = 0;
        dst->y = (win_h - dst->h) / 2;
    }
}

/* ================================================================
 * Mouse coordinate conversion: window space → ROM space
 * ================================================================ */

static void convert_mouse_coords(int wx, int wy, int *rx, int *ry,
                                 int win_w, int win_h, uint32_t W, uint32_t H) {
    SDL_Rect dst;
    calc_letterbox(win_w, win_h, W, H, &dst);

    /* Map window coords into ROM pixel coords */
    float scale_x = (float)W / (float)dst.w;
    float scale_y = (float)H / (float)dst.h;

    *rx = (int)((wx - dst.x) * scale_x);
    *ry = (int)((wy - dst.y) * scale_y);

    /* Clamp to ROM bounds */
    if (*rx < 0) *rx = 0;
    if (*rx >= (int)W) *rx = (int)W - 1;
    if (*ry < 0) *ry = 0;
    if (*ry >= (int)H) *ry = (int)H - 1;
}

/* ================================================================
 * Resolve the runtime memory base pointer
 * ================================================================ */

static void refresh_memory(void) {
    g_mem = m3_GetMemory(g_runtime, &g_mem_len, 0);
}

/* ================================================================
 * Read screen configuration with defaults
 * ================================================================ */

static void read_screen_config(uint32_t *W, uint32_t *H, uint32_t *BPP, uint32_t *SCALE) {
    *W     = read_u32(g_g_w_width);
    *H     = read_u32(g_g_w_height);
    *BPP   = read_u32(g_g_w_bpp);
    *SCALE = read_u32(g_g_w_scale);
    if (*W == 0)     *W = 320;
    if (*H == 0)     *H = 240;
    if (*BPP == 0)   *BPP = 32;
    if (*SCALE == 0) *SCALE = 1;
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    /* ---- Load WASM binary ---- */
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("Failed to open ROM"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *wasm_data = (uint8_t *)malloc(sz);
    if (!wasm_data) { fprintf(stderr, "Out of memory\n"); fclose(f); return 1; }
    fread(wasm_data, 1, sz, f);
    fclose(f);

    /* ---- Initialize wasm3 ---- */
    IM3Environment env = m3_NewEnvironment();
    if (!env) { fprintf(stderr, "m3_NewEnvironment failed\n"); free(wasm_data); return 1; }

    g_runtime = m3_NewRuntime(env, 64 * 1024 * 1024, NULL);
    if (!g_runtime) { fprintf(stderr, "m3_NewRuntime failed\n"); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    M3Result result = m3_ParseModule(env, &g_module, wasm_data, sz);
    if (result) { fprintf(stderr, "Parse error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    result = m3_LoadModule(g_runtime, g_module);
    if (result) { fprintf(stderr, "Load error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    result = m3_LinkLibC(g_module);
    if (result) fprintf(stderr, "Warning: m3_LinkLibC: %s\n", result);

    /* Link strlen into "env" module */
    result = m3_LinkRawFunction(g_module, "env", "strlen", "i(i)", &m3_libc_strlen);
    if (result) fprintf(stderr, "Warning: strlen link: %s\n", result);

    /* Math primitives — provided by glibc. OGG ROMs (stb_vorbis) need
     * these for the inverse MDCT; MP3/WAV ROMs do not import them.
     * Note: 'i' (lowercase) = i32, 'I' = i64. */
    result = m3_LinkRawFunction(g_module, "env", "sin",   "F(F)",  &m3_env_sin);   if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "cos",   "F(F)",  &m3_env_cos);   if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "exp",   "F(F)",  &m3_env_exp);   if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "log",   "F(F)",  &m3_env_log);   if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "pow",   "F(FF)", &m3_env_pow);   if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "ldexp", "F(Fi)", &m3_env_ldexp); if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "fabs",  "F(F)",  &m3_env_fabs);  if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "floor", "F(F)",  &m3_env_floor); if (result) {}
    result = m3_LinkRawFunction(g_module, "env", "ceil",  "F(F)",  &m3_env_ceil);  if (result) {}

    /* ---- Find wupdate ---- */
    IM3Function f_wupdate = NULL;
    result = m3_FindFunction(&f_wupdate, g_runtime, "wupdate");
    if (result || !f_wupdate) {
        fprintf(stderr, "ROM does not export wupdate(): %s\n", result ? result : "not found");
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Cache all global pointers ---- */
    cache_globals();

    /* ---- Refresh WASM memory ---- */
    refresh_memory();

    /* ---- Initialize SDL ---- */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    init_pixel_luts();

    /* Audio buffer mutex — created after SDL_Init so SDL_CreateMutex
     * is available; checked in the callback for NULL so an early audio
     * callback (before we get here) just runs without locking. */
    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        SDL_Quit(); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Read initial config ---- */
    uint32_t W, H, BPP, SCALE;
    read_screen_config(&W, &H, &BPP, &SCALE);

    char title[128];
    read_str(g_g_w_title, title, 128);

    /* ---- Create window ---- */
    SDL_Window *window = SDL_CreateWindow(
        title[0] ? title : "Untitled",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(W * SCALE), (int)(H * SCALE),
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit(); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Create renderer (HW accel + vsync, fallback to software) ---- */
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit();
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Create texture ---- */
    SDL_Texture *texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
        (int)W, (int)H);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    /* ---- Audio device ---- */
    SDL_AudioDeviceID audio_dev = 0;
    uint32_t prev_audio_size     = read_u32(g_g_w_audio_size);
    uint32_t prev_audio_rate     = read_u32(g_g_w_audio_sample_rate);
    uint32_t prev_audio_bpp      = read_u32(g_g_w_audio_bpp);
    uint32_t prev_audio_channels = read_u32(g_g_w_audio_channels);

    if (prev_audio_size > 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq     = prev_audio_rate ? prev_audio_rate : 44100;
        wanted.format   = AUDIO_F32;
        wanted.channels = prev_audio_channels ? prev_audio_channels : 1;
        wanted.samples  = 1024;
        wanted.callback = host_audio_callback;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }

    /* ---- Track previous config for change detection ---- */
    uint32_t prev_W = W, prev_H = H, prev_BPP = BPP, prev_SCALE = SCALE;
    uint8_t keys_state[256];
    memset(keys_state, 0, sizeof(keys_state));

    uint32_t mouse_buttons = 0;
    int      mouse_x = 0, mouse_y = 0;
    int      mouse_wheel = 0;
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
                if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    /* Window was resized by user — no action needed,
                     * letterbox is computed dynamically */
                }
                break;
            }
        }

        /* ---- Step 1: Write input to WASM globals ---- */

        /* Keys array */
        {
            uint8_t *keys_ptr = ptr_u8(g_g_w_keys);
            if (keys_ptr) {
                memcpy(keys_ptr, keys_state, 256);
            }
        }
        write_i32(g_g_w_mouse_x, mouse_x);
        write_i32(g_g_w_mouse_y, mouse_y);
        write_u32(g_g_w_mouse_buttons, mouse_buttons);
        write_i32(g_g_w_mouse_wheel, mouse_wheel);
        write_u32(g_g_w_gamepad_buttons, gamepad_buttons);
        write_u32(g_g_w_ticks, SDL_GetTicks());

        /* ---- Step 2: Call wupdate(), exit if 0 ----
         *
         * Hold the audio mutex for the whole wupdate call. The ROM's
         * fill_audio writes to w_audio_buffer inside wupdate; holding
         * the lock here prevents the audio thread from reading while
         * the write is in progress. */
        {
            int32_t keep = 0;
            if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
            m3_CallV(f_wupdate);
            m3_GetResultsV(f_wupdate, &keep);
            if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
            if (!keep) break;
        }

        /* ---- Refresh memory pointer (ROM may have grown it) ---- */
        refresh_memory();

        /* ---- Step 3: Read config and detect changes ---- */
        read_screen_config(&W, &H, &BPP, &SCALE);

        int config_changed = (W != prev_W || H != prev_H ||
                              BPP != prev_BPP || SCALE != prev_SCALE);

        char new_title[128];
        read_str(g_g_w_title, new_title, 128);
        int title_changed = (strcmp(new_title, title) != 0);

        if (config_changed || title_changed) {
            SDL_SetWindowSize(window, (int)(W * SCALE), (int)(H * SCALE));
            if (title_changed) {
                SDL_SetWindowTitle(window, new_title[0] ? new_title : "Untitled");
                strncpy(title, new_title, sizeof(title) - 1);
                title[sizeof(title) - 1] = '\0';
            }
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer,
                SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                (int)W, (int)H);
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
            prev_W = W; prev_H = H; prev_BPP = BPP; prev_SCALE = SCALE;

            /* Recompute mouse position for new resolution */
            {
                int win_w, win_h;
                SDL_GetWindowSize(window, &win_w, &win_h);
                convert_mouse_coords(mouse_x, mouse_y, &mouse_x, &mouse_y,
                                     win_w, win_h, W, H);
            }
        }

        /* ---- Detect audio config changes ---- */
        {
            uint32_t cur_size     = read_u32(g_g_w_audio_size);
            uint32_t cur_rate     = read_u32(g_g_w_audio_sample_rate);
            uint32_t cur_bpp      = read_u32(g_g_w_audio_bpp);
            uint32_t cur_channels = read_u32(g_g_w_audio_channels);

            if (cur_size != prev_audio_size || cur_rate != prev_audio_rate ||
                cur_bpp != prev_audio_bpp || cur_channels != prev_audio_channels) {

                if (audio_dev) {
                    SDL_CloseAudioDevice(audio_dev);
                    audio_dev = 0;
                }

                if (cur_size > 0 && cur_rate > 0 && cur_channels > 0) {
                    SDL_AudioSpec wanted;
                    SDL_zero(wanted);
                    wanted.freq     = cur_rate;
                    wanted.format   = AUDIO_F32;
                    wanted.channels = cur_channels;
                    wanted.samples  = 1024;
                    wanted.callback = host_audio_callback;
                    audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
                    if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
                }

                prev_audio_size     = cur_size;
                prev_audio_rate     = cur_rate;
                prev_audio_bpp      = cur_bpp;
                prev_audio_channels = cur_channels;
            }
        }

        /* ---- Step 4: Render dirty rects ---- */
        {
            uint8_t *vram = ptr_u8(g_g_w_vram);
            uint32_t dirty_count = read_u32(g_g_w_dirty_count);

            if (vram && dirty_count > 0) {
                if (dirty_count == 1) {
                    /* Fast path: check if single rect covers entire screen */
                    int rx, ry, rw, rh;
                    read_dirty_rect(0, &rx, &ry, &rw, &rh);
                    if (rx == 0 && ry == 0 &&
                        (uint32_t)rw == W && (uint32_t)rh == H) {
                        render_fullscreen(texture, vram, W, H, BPP);
                    } else {
                        render_rect_to_texture(texture, vram,
                            rx, ry, rw, rh, W, H, BPP);
                    }
                } else {
                    /* Multiple dirty rects (up to 32) */
                    uint32_t count = dirty_count;
                    if (count > 32) count = 32;
                    for (uint32_t i = 0; i < count; i++) {
                        int rx, ry, rw, rh;
                        read_dirty_rect((int)i, &rx, &ry, &rw, &rh);
                        render_rect_to_texture(texture, vram,
                            rx, ry, rw, rh, W, H, BPP);
                    }
                }

                /* Calculate letterbox destination */
                int win_w, win_h;
                SDL_GetWindowSize(window, &win_w, &win_h);
                SDL_Rect dst;
                calc_letterbox(win_w, win_h, W, H, &dst);

                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, &dst);
                SDL_RenderPresent(renderer);

                /* Reset dirty count */
                write_u32(g_g_w_dirty_count, 0);
            }
        }

        /* ---- Step 5: Reset mouse wheel ---- */
        write_i32(g_g_w_mouse_wheel, 0);
        mouse_wheel = 0;

        /* ---- Yield / FPS limit ---- */
        uint32_t target_fps = 0;
        if (g_g_w_target_fps) {
            M3TaggedValue tv;
            if (!m3_GetGlobal(g_g_w_target_fps, &tv)) target_fps = (uint32_t)tv.value.i32;
        }
        if (target_fps > 0) {
            static uint32_t frame_start = 0;
            uint32_t now = SDL_GetTicks();
            uint32_t elapsed = now - frame_start;
            int32_t delay = (1000 / target_fps) - (int32_t)elapsed;
            if (delay > 0) SDL_Delay((uint32_t)delay);
            frame_start = SDL_GetTicks();
        } else {
            SDL_Delay(1);
        }
    }

    /* ================================================================
     * Cleanup
     * ================================================================ */

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    /* Audio callback is already stopped (SDL_CloseAudioDevice above), so
     * no other thread holds the mutex. Safe to destroy. */
    if (g_audio_mutex) SDL_DestroyMutex(g_audio_mutex);
    g_audio_mutex = NULL;

    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
