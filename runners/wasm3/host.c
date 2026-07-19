/*
 * Wagnostic Reference Runner — wasm3 + SDL2 host
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
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

/* ================================================================
 * WagnosticState struct (must match wagnostic.h exactly)
 * ================================================================ */

typedef struct { int x, y, w, h; } Rect;

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
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint32_t is_signed, is_float, is_shared_exponent;
    uint8_t reserved[504];
} WagnosticState;

// ============================================================
// Format Decoders
// ============================================================

static inline double decodeFloat16(uint16_t binary) {
    int sign = (binary & 0x8000) ? -1 : 1;
    int exp = (binary & 0x7C00) >> 10;
    int frac = binary & 0x03FF;
    if (exp == 0) {
        if (frac == 0) return 0.0;
        return sign * pow(2.0, -14.0) * (frac / 1024.0);
    } else if (exp == 0x1F) {
        return frac == 0 ? sign * INFINITY : NAN;
    }
    return sign * pow(2.0, exp - 15.0) * (1.0 + frac / 1024.0);
}

static inline double decodeSharedExp(uint64_t val, uint8_t bits, uint8_t shift, uint8_t totalExp) {
    uint64_t mantissa = (val >> shift) & ((1ULL << bits) - 1ULL);
    double norm = (double)mantissa / (double)((1ULL << bits) - 1ULL);
    int exp = (int)totalExp - 15;
    return norm * pow(2.0, exp);
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
    if (max_bit <= 64) return 64;
    if (max_bit <= 128) return 128;
    return 256;
}

static inline uint64_t get_mask(uint8_t bits) {
    return (bits >= 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bits) - 1ULL);
}

static inline uint8_t extractChannel(uint64_t px, uint64_t px2, uint64_t px3, uint64_t px4, uint8_t bits, uint8_t shift, bool is_shared_exp, bool is_float, bool is_signed, uint8_t ab, uint8_t a_shift) {
    if (!bits) return 0;
    uint64_t val = 0;
    if (shift < 64) val = (px >> shift) & get_mask(bits);
    else if (shift < 128) val = (px2 >> (shift - 64)) & get_mask(bits);
    else if (shift < 192) val = (px3 >> (shift - 128)) & get_mask(bits);
    else val = (px4 >> (shift - 192)) & get_mask(bits);

    if (is_shared_exp) {
        uint64_t sharedExp = (px >> a_shift) & get_mask(ab);
        double floatVal = decodeSharedExp(px, bits, shift, (uint8_t)sharedExp);
        int mapped = (int)(floatVal * 255.0);
        return mapped < 0 ? 0 : (mapped > 255 ? 255 : mapped);
    }

    if (is_float) {
        double f = 0.0;
        if (bits == 16) f = decodeFloat16((uint16_t)val);
        else if (bits == 32) {
            float f32; memcpy(&f32, &val, 4); f = f32;
        } else if (bits == 64) {
            double f64; memcpy(&f64, &val, 8); f = f64;
        }
        int mapped = (int)(f * 255.0);
        return mapped < 0 ? 0 : (mapped > 255 ? 255 : mapped);
    }

    if (is_signed) {
        uint64_t maxVal = get_mask(bits - 1);
        uint64_t signBit = (val >> (bits - 1)) & 1ULL;
        int64_t sVal = val;
        if (signBit) sVal -= (1ULL << bits);
        int mapped = (int)(sVal * 255 / (int64_t)maxVal);
        return mapped < 0 ? 0 : (mapped > 255 ? 255 : mapped);
    }

    uint64_t maxVal = get_mask(bits);
    return (uint8_t)(val * 255 / maxVal);
}

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




static_assert(sizeof(WagnosticState) == 1024, "WagnosticState size mismatch — check struct layout");

/* ================================================================
 * Globals
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

static uint8_t *g_mem     = NULL;
static uint32_t g_mem_len = 0;

/* Latest state pointer (offset into WASM memory). Updated each frame.
 * The audio callback reads from this pointer. */
static uint32_t g_state_ptr = 0;



/* Audio buffer lock */
static SDL_mutex *g_audio_mutex = NULL;

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

static inline uint8_t *get_audio_buffer(WagnosticState *s) {
    if (!s || s->audio_buffer_offset == 0) return NULL;
    return (uint8_t *)s + s->audio_buffer_offset;
}



/* ================================================================
 * Screen config with defaults
 * ================================================================ */

static void read_screen_config(WagnosticState *s,
                               uint32_t *W, uint32_t *H,
                               uint32_t *BPP, uint32_t *SCALE) {
    *W     = s ? s->width  : 0;
    *H     = s ? s->height : 0;
    *BPP   = compute_bpp(s);
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
    
    uint32_t r_b = s ? s->r_bits : 0;
    uint32_t r_s = s ? s->r_shift : 0;
    uint32_t g_b = s ? s->g_bits : 0;
    uint32_t g_s = s ? s->g_shift : 0;
    uint32_t b_b = s ? s->b_bits : 0;
    uint32_t b_s = s ? s->b_shift : 0;
    uint32_t a_b = s ? s->a_bits : 0;
    uint32_t a_s = s ? s->a_shift : 0;

    
    if (!r_b && !g_b && !b_b && !a_b) {
        if (BPP == 32) {
            a_b = 8; a_s = 24; b_b = 8; b_s = 16; g_b = 8; g_s = 8; r_b = 8; r_s = 0;
        } else if (BPP == 24) {
            b_b = 8; b_s = 16; g_b = 8; g_s = 8; r_b = 8; r_s = 0;
        } else if (BPP == 16) {
            r_b = 5; r_s = 11; g_b = 6; g_s = 5; b_b = 5; b_s = 0;
        } else if (BPP == 8) {
            r_b = 3; r_s = 5; g_b = 3; g_s = 2; b_b = 2; b_s = 0;
        } else if (BPP == 4 || BPP == 2 || BPP == 1) {
            a_b = BPP; a_s = 0;
        }
    }

    int is_grayscale = (!r_b && !g_b && !b_b && a_b);

    for (int y = ry; y < ry + rh; y++) {
        uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
        for (int x = rx; x < rx + rw; x++) {
            uint64_t px = 0, px2 = 0, px3 = 0, px4 = 0;
            size_t idx = y * W + x;
            if (BPP == 256) {
                uint64_t* p = (uint64_t*)(vram + idx * 32);
                px = p[0]; px2 = p[1]; px3 = p[2]; px4 = p[3];
            } else if (BPP == 128) {
                uint64_t* p = (uint64_t*)(vram + idx * 16);
                px = p[0]; px2 = p[1];
            } else if (BPP == 64) px = ((uint64_t*)vram)[idx];
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
            bool is_float = s ? s->is_float : 0;
            bool is_signed = s ? s->is_signed : 0;
            bool is_shared_exp = s ? s->is_shared_exponent : 0;
            
            if (is_grayscale) {
                uint32_t lum = extractChannel(px, px2, px3, px4, a_b, a_s, is_shared_exp, is_float, is_signed, a_b, a_s);
                r = g = b = lum;
                a = 255;
            } else {
                r = extractChannel(px, px2, px3, px4, r_b, r_s, is_shared_exp, is_float, is_signed, a_b, a_s);
                g = extractChannel(px, px2, px3, px4, g_b, g_s, is_shared_exp, is_float, is_signed, a_b, a_s);
                b = extractChannel(px, px2, px3, px4, b_b, b_s, is_shared_exp, is_float, is_signed, a_b, a_s);
                if (a_b && !is_shared_exp) {
                    a = extractChannel(px, px2, px3, px4, a_b, a_s, 0, is_float, is_signed, a_b, a_s);
                }
            }
            dst[x - rx] = (a << 24) | (b << 16) | (g << 8) | r;
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
 * Audio callback
 * ================================================================ */

static void host_audio_callback(void *userdata, Uint8 *stream_ptr, int len_bytes) {
    (void)userdata;
    float *stream = (float *)stream_ptr;
    int nsamples  = len_bytes / (int)sizeof(float);

    if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);

    WagnosticState *s = get_state();
    uint8_t *abuf = get_audio_buffer(s);
    uint32_t size  = s ? s->audio_size  : 0;
    uint32_t bpp   = s ? s->audio_bpp   : 0;
    uint32_t r_off = s ? s->audio_read  : 0;
    uint32_t w_off = s ? s->audio_write : 0;

    if (!abuf || size == 0 || bpp == 0 || bpp > 4) {
        memset(stream, 0, len_bytes);
        if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
        return;
    }

    if (r_off >= size) r_off = 0;
    if (w_off >= size) w_off = 0;

    uint32_t avail;
    if (w_off >= r_off) avail = w_off - r_off;
    else                avail = size - r_off + w_off;

    uint32_t max_bytes = avail;
    uint32_t stream_bytes = (uint32_t)nsamples * bpp;
    if (max_bytes > stream_bytes) max_bytes = stream_bytes;
    max_bytes = (max_bytes / bpp) * bpp;

    uint32_t bytes_read = 0;
    int stream_idx = 0;

    while (bytes_read < max_bytes && stream_idx < nsamples) {
        uint32_t chunk = max_bytes - bytes_read;
        uint32_t pos = (r_off + bytes_read) % size;
        uint32_t to_end = size - pos;
        if (chunk > to_end) chunk = to_end;

        if (bpp == 1) {
            for (uint32_t b = 0; b < chunk && stream_idx < nsamples; b++) {
                stream[stream_idx++] = ((float)abuf[pos + b] - 128.0f) / 128.0f;
            }
        } else if (bpp == 2) {
            for (uint32_t b = 0; b + 1 < chunk && stream_idx < nsamples; b += 2) {
                int16_t s16;
                memcpy(&s16, abuf + pos + b, 2);
                stream[stream_idx++] = (float)s16 / 32768.0f;
            }
        } else {
            for (uint32_t b = 0; b + 3 < chunk && stream_idx < nsamples; b += 4) {
                float sample;
                memcpy(&sample, abuf + pos + b, 4);
                stream[stream_idx++] = sample;
            }
        }
        bytes_read += chunk;
    }

    int underrun = (stream_idx < nsamples);
    for (int i = stream_idx; i < nsamples; i++) stream[i] = 0.0f;

    if (bytes_read > 0 && s) {
        s->audio_read = (r_off + bytes_read) % size;
    }
    if (underrun && s) {
        s->audio_underrun++;
    }

    if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
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
        fprintf(stderr, "Usage: %s <rom.wasm|rom.wag>\n", argv[0]);
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }


    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        SDL_Quit(); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Call wupdate() once to get initial state pointer ---- */
    {
        if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
        m3_CallV(f_wupdate);
        m3_GetResultsV(f_wupdate, &g_state_ptr);
        if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
    }
    refresh_memory();

    WagnosticState *state = get_state();

    /* ---- Read initial config ---- */
    uint32_t W, H, BPP, SCALE;
    read_screen_config(state, &W, &H, &BPP, &SCALE);

    char title[128];
    if (state) {
        strncpy(title, state->title, 127);
        title[127] = '\0';
    } else {
        title[0] = '\0';
    }

    /* ---- Create window ---- */
    // window logging removed
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

    /* ---- Audio device ---- */
    SDL_AudioDeviceID audio_dev = 0;
    uint32_t prev_audio_size = state ? state->audio_size : 0;
    uint32_t prev_audio_rate = state ? state->audio_sample_rate : 0;
    uint32_t prev_audio_bpp  = state ? state->audio_bpp : 0;
    uint32_t prev_audio_channels = state ? state->audio_channels : 0;

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
        }

        /* ---- Step 2: Call wupdate(), exit if return is 0 ---- */
        {
            int32_t keep = 0;
            if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
            m3_CallV(f_wupdate);
            m3_GetResultsV(f_wupdate, &keep);
            if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
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

        char new_title[128];
        if (state) {
            strncpy(new_title, state->title, 127);
            new_title[127] = '\0';
        } else {
            new_title[0] = '\0';
        }
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

            int win_w, win_h;
            SDL_GetWindowSize(window, &win_w, &win_h);
            convert_mouse_coords(mouse_x, mouse_y, &mouse_x, &mouse_y,
                                 win_w, win_h, W, H);
        }

        /* ---- Detect audio config changes ---- */
        if (state) {
            uint32_t cur_size     = state->audio_size;
            uint32_t cur_rate     = state->audio_sample_rate;
            uint32_t cur_bpp      = state->audio_bpp;
            uint32_t cur_channels = state->audio_channels;

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
        if (state) {
            uint8_t *vram = get_vram(state);

            if (vram) {
                if (state->dirty_rects == 0) {
                    render_fullscreen(texture, vram, state, W, H, BPP);
                } else {
                    uint32_t* rect_data = (uint32_t*)(g_mem + state->dirty_rects);
                    uint32_t count = rect_data[0];
                    Rect* rects = (Rect*)(rect_data + 1);
                    
                    for (uint32_t i = 0; i < count; i++) {
                        int rx = rects[i].x;
                        int ry = rects[i].y;
                        int rw = rects[i].w;
                        int rh = rects[i].h;
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

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (g_audio_mutex) SDL_DestroyMutex(g_audio_mutex);
    g_audio_mutex = NULL;
    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
