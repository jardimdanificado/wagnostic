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
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    Rect dirty_rects[32];
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
    uint32_t io_load;             // +984
    uint32_t io_load_buffer;      // +988
    uint32_t io_load_size;        // +992
    uint32_t io_save;             // +996
    uint32_t io_save_buffer;      // +1000
    uint32_t io_save_size;        // +1004
    uint8_t reserved[16];         // +1008
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

static size_t tar_get_file_size(const char* tar_path, const char* target_filename) {
    FILE* f = fopen(tar_path, "rb");
    if (!f) return 0;
    uint8_t header[512];
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
            best_sz = size;
        }
        long skip = size + ((512 - (size % 512)) % 512);
        fseek(f, skip, SEEK_CUR);
    }
    fclose(f);
    return best_sz;
}

static void tar_append_file(const char* tar_path, const char* target_filename, uint8_t* data, size_t size) {
    FILE* f = fopen(tar_path, "r+b");
    if (!f) return;
    uint8_t header[512];
    long last_good_pos = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') {
            break;
        }
        size_t fsize = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                fsize = fsize * 8 + (header[124+i] - '0');
        }
        long skip = fsize + ((512 - (fsize % 512)) % 512);
        fseek(f, skip, SEEK_CUR);
        last_good_pos = ftell(f);
    }
    fseek(f, last_good_pos, SEEK_SET);
    memset(header, 0, 512);
    strncpy((char*)header, target_filename, 99);
    sprintf((char*)header + 100, "%07o", 0644);
    sprintf((char*)header + 124, "%011zo", size);
    header[135] = ' '; 
    strcpy((char*)header + 257, "ustar  ");
    header[156] = '0'; 
    memset(header + 148, ' ', 8);
    
    uint32_t checksum = 0;
    for (int i = 0; i < 512; i++) {
        checksum += header[i];
    }
    sprintf((char*)header + 148, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    
    fwrite(header, 1, 512, f);
    fwrite(data, 1, size, f);
    uint8_t zeros[1024] = {0};
    long remainder = (512 - (size % 512)) % 512;
    if (remainder > 0) fwrite(zeros, 1, remainder, f);
    fwrite(zeros, 1, 1024, f);
    fclose(f);
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

/* Pixel lookup tables (LUTs) for fast format conversion */
static uint32_t rgb332_lut[256];
static uint32_t rgb565_lut[65536];
static int pixel_lut_initialized = 0;

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
    *BPP   = s ? s->bpp    : 0;
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
 * Render helpers
 * ================================================================ */

static void render_rect_to_texture(SDL_Texture *texture, uint8_t *vram,
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

    if (BPP == 8) {
        for (int y = ry; y < ry + rh; y++) {
            uint8_t  *src = vram + y * W + rx;
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            for (int x = 0; x < rw; x++) dst[x] = rgb332_lut[src[x]];
        }
    } else if (BPP == 16) {
        for (int y = ry; y < ry + rh; y++) {
            uint16_t *src = (uint16_t *)(vram + (y * W + rx) * 2);
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            for (int x = 0; x < rw; x++) dst[x] = rgb565_lut[src[x]];
        }
    } else if (BPP == 24) {
        for (int y = ry; y < ry + rh; y++) {
            uint8_t *src = vram + ((y * W + rx) * 3);
            uint32_t *dst = (uint32_t *)((uint8_t *)pixels + (y - ry) * pitch);
            for (int x = 0; x < rw; x++) {
                uint8_t r = src[x*3];
                uint8_t g = src[x*3 + 1];
                uint8_t b = src[x*3 + 2];
                dst[x] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u;
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

static void render_fullscreen(SDL_Texture *texture, uint8_t *vram,
                              uint32_t W, uint32_t H, uint32_t BPP) {
    void *pixels;
    int pitch;
    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    if (BPP == 8) {
        uint8_t  *src = vram;
        uint32_t *dst = (uint32_t *)pixels;
        uint32_t total = W * H;
        for (uint32_t i = 0; i < total; i++) dst[i] = rgb332_lut[src[i]];
    } else if (BPP == 16) {
        uint16_t *src = (uint16_t *)vram;
        uint32_t *dst = (uint32_t *)pixels;
        uint32_t total = W * H;
        for (uint32_t i = 0; i < total; i++) dst[i] = rgb565_lut[src[i]];
    } else if (BPP == 24) {
        uint8_t *src = vram;
        uint32_t *dst = (uint32_t *)pixels;
        uint32_t total = W * H;
        for (uint32_t i = 0; i < total; i++) {
            uint8_t r = src[i*3];
            uint8_t g = src[i*3 + 1];
            uint8_t b = src[i*3 + 2];
            dst[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u;
        }
    } else if (BPP == 32) {
        memcpy(pixels, vram, W * H * 4);
    }
    SDL_UnlockTexture(texture);
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

    init_pixel_luts();
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

        /* ---- Process IO Streams ---- */
        if (state && g_is_tar) {
            // Process Load
            if (state->io_load && state->io_load < g_mem_len) {
                const char *path = (const char *)(g_mem + state->io_load);
                size_t file_sz = tar_get_file_size(g_rom_path, path);
                if (file_sz > 0) {
                    if (state->io_load_buffer == 0) {
                        // Probe phase
                        state->io_load_size = (uint32_t)file_sz;
                    } else if (state->io_load_buffer + file_sz <= g_mem_len) {
                        // Read phase
                        size_t exact_sz = 0;
                        uint8_t *data = tar_extract_file(g_rom_path, path, &exact_sz);
                        if (data) {
                            memcpy(g_mem + state->io_load_buffer, data, exact_sz);
                            free(data);
                        }
                    }
                } else {
                    // Not found
                    if (state->io_load_buffer == 0) state->io_load_size = 0;
                }
                state->io_load = 0; // consumed
            }

            // Process Save
            if (state->io_save && state->io_save < g_mem_len) {
                const char *path = (const char *)(g_mem + state->io_save);
                if (state->io_save_buffer > 0 && state->io_save_buffer + state->io_save_size <= g_mem_len) {
                    tar_append_file(g_rom_path, path, g_mem + state->io_save_buffer, state->io_save_size);
                    printf("Saved to %s\n", g_rom_path);
                }
                state->io_save = 0; // consumed
            }
        }

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
            uint32_t dirty_count = state->dirty_count;

            if (vram && dirty_count > 0) {
                if (dirty_count == 1) {
                    int rx = state->dirty_rects[0].x;
                    int ry = state->dirty_rects[0].y;
                    int rw = state->dirty_rects[0].w;
                    int rh = state->dirty_rects[0].h;
                    if (rx == 0 && ry == 0 &&
                        (uint32_t)rw == W && (uint32_t)rh == H) {
                        render_fullscreen(texture, vram, W, H, BPP);
                    } else {
                        render_rect_to_texture(texture, vram,
                            rx, ry, rw, rh, W, H, BPP);
                    }
                } else {
                    uint32_t count = dirty_count;
                    if (count > 32) count = 32;
                    for (uint32_t i = 0; i < count; i++) {
                        int rx = state->dirty_rects[i].x;
                        int ry = state->dirty_rects[i].y;
                        int rw = state->dirty_rects[i].w;
                        int rh = state->dirty_rects[i].h;
                        render_rect_to_texture(texture, vram,
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

                state->dirty_count = 0;
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
