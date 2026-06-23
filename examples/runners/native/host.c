#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t scale;
    uint32_t audio_size;
    uint32_t audio_write;
    uint32_t audio_read;
    uint32_t audio_rate;
    uint32_t audio_bpp;
    uint32_t audio_channels;
    uint32_t ticks;
    uint32_t gamepad;
    int32_t  jx, jy, rx, ry;
    uint8_t  keys[256];
    int32_t  mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;
    uint8_t  signals[4];
    uint8_t  reserved[44];
} SystemConfig;
#pragma pack(pop)

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_textures[3];
static SDL_Surface* convert_surface = NULL;
static int tex_idx = 0;
static SDL_AudioDeviceID audio_dev = 0;
static uint32_t W=320, H=240, BPP=8, SCALE=1;
static IM3Runtime runtime = NULL;

static void convert_mouse_coords(int window_x, int window_y, int* rom_x, int* rom_y) {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    
    // Calculate destination rectangle (same as rendering)
    SDL_Rect dst;
    if (aspect_win > aspect_rom) {
        dst.h = win_h;
        dst.w = (int)(win_h * aspect_rom);
        dst.x = (win_w - dst.w) / 2;
        dst.y = 0;
    } else {
        dst.w = win_w;
        dst.h = (int)(win_w / aspect_rom);
        dst.x = 0;
        dst.y = (win_h - dst.h) / 2;
    }
    
    // Convert window coordinates to ROM coordinates
    float scale_x = (float)W / dst.w;
    float scale_y = (float)H / dst.h;
    
    *rom_x = (int)((window_x - dst.x) * scale_x);
    *rom_y = (int)((window_y - dst.y) * scale_y);
    
    // Clamp to ROM bounds
    if (*rom_x < 0) *rom_x = 0;
    if (*rom_x >= W) *rom_x = W - 1;
    if (*rom_y < 0) *rom_y = 0;
    if (*rom_y >= H) *rom_y = H - 1;
}

void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)mem;
    uint8_t* audio_buf = mem + 512 + (W * H * (BPP / 8));
    float* stream = (float*)stream_ptr;
    int num_samples = len_bytes / sizeof(float);

    uint32_t r = sys->audio_read;
    uint32_t w = sys->audio_write;
    uint32_t size = sys->audio_size;
    if (size == 0) return;

    for (int i = 0; i < num_samples; i++) {
        if (r == w) { stream[i] = 0.0f; continue; }
        float sample = 0;
        if (sys->audio_bpp == 1) {
            sample = (audio_buf[r] - 128) / 128.0f;
            r = (r + 1) % size;
        } else if (sys->audio_bpp == 2) {
            sample = (*(int16_t*)(audio_buf + r)) / 32768.0f;
            r = (r + 2) % size;
        } else if (sys->audio_bpp == 4) {
            sample = (*(float*)(audio_buf + r));
            r = (r + 4) % size;
        }
        stream[i] = sample;
    }
    sys->audio_read = r;
}

static void convert_8bpp_to_rgba(const uint8_t* src, uint8_t* dst, int w, int h) {
    for (int i = 0; i < w * h; i++) {
        uint8_t raw = src[i];
        dst[i*4+0] = ((raw >> 5) & 0x07) * 255 / 7;
        dst[i*4+1] = ((raw >> 2) & 0x07) * 255 / 7;
        dst[i*4+2] = ( raw       & 0x03) * 255 / 3;
        dst[i*4+3] = 255;
    }
}

static void convert_16bpp_to_rgba(const uint8_t* src, uint8_t* dst, int w, int h) {
    const uint16_t* src16 = (const uint16_t*)src;
    for (int i = 0; i < w * h; i++) {
        uint16_t raw = src16[i];
        dst[i*4+0] = ((raw >> 11) & 0x1F) * 255 / 31;
        dst[i*4+1] = ((raw >>  5) & 0x3F) * 255 / 63;
        dst[i*4+2] = ( raw        & 0x1F) * 255 / 31;
        dst[i*4+3] = 255;
    }
}

static void convert_32bpp_to_rgba(const uint8_t* src, uint8_t* dst, int w, int h) {
    memcpy(dst, src, w * h * 4);
}

static void init_sdl_from_header() {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)mem;

    W = sys->width; H = sys->height; BPP = sys->bpp; SCALE = sys->scale;
    if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 8; if (SCALE == 0) SCALE = 1;

    if (!window) {
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            W * SCALE, H * SCALE, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) return;
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
    } else {
        SDL_SetWindowSize(window, W * SCALE, H * SCALE);
    }

    for (int i = 0; i < 3; i++) {
        if (screen_textures[i]) SDL_DestroyTexture(screen_textures[i]);
        screen_textures[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                                SDL_TEXTUREACCESS_STREAMING, W, H);
        SDL_SetTextureScaleMode(screen_textures[i], SDL_ScaleModeNearest);
    }
    tex_idx = 0;

    if (convert_surface) SDL_FreeSurface(convert_surface);
    convert_surface = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ABGR8888);

    if (sys->audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = sys->audio_rate ? sys->audio_rate : 44100;
        wanted.format = AUDIO_F32;
        wanted.channels = sys->audio_channels ? sys->audio_channels : 1;
        wanted.samples = 1024;
        wanted.callback = host_audio_callback;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("Failed to open ROM"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);

    IM3Environment env = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 64*1024*1024, NULL);
    IM3Module module;
    m3_ParseModule(env, &module, wasm_data, sz);
    m3_LoadModule(runtime, module);

    IM3Function f_init = NULL, f_upd = NULL;
    m3_FindFunction(&f_init, runtime, "winit");
    m3_FindFunction(&f_upd, runtime, "wupdate");

    if (f_init) m3_CallV(f_init);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    init_sdl_from_header();

    int running = 1;
    while (running) {
        SDL_Event ev;
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) { SDL_Delay(1); continue; }
        SystemConfig* sys = (SystemConfig*)mem;
        sys->ticks = SDL_GetTicks();

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (ev.key.keysym.scancode < 256)
                    sys->keys[ev.key.keysym.scancode] = (ev.type == SDL_KEYDOWN);
            }
            if (ev.type == SDL_MOUSEMOTION) {
                convert_mouse_coords(ev.motion.x, ev.motion.y, &sys->mouse_x, &sys->mouse_y);
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                int b = ev.button.button;
                if (b == SDL_BUTTON_LEFT)
                    sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 1) : (sys->mouse_buttons & ~1);
                if (b == SDL_BUTTON_RIGHT)
                    sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 2) : (sys->mouse_buttons & ~2);
            }
            if (ev.type == SDL_MOUSEWHEEL) {
                sys->mouse_wheel += ev.wheel.y;
            }
        }

        if (f_upd) m3_CallV(f_upd);

        mem = m3_GetMemory(runtime, NULL, 0);
        sys = (SystemConfig*)mem;

        int redraw = 0;
        for (int i = 0; i < 4; i++) {
            uint8_t s = sys->signals[i];
            if (s == 1) redraw = 1;
            else if (s == 2) running = 0;
            else if (s >= 3 && s <= 5) init_sdl_from_header();
            sys->signals[i] = 0;
        }
        sys->mouse_wheel = 0;

        if (redraw) {
            uint8_t* vram = mem + 512;

            int cur_tex = tex_idx;
            tex_idx = (tex_idx + 1) % 3;

            void* pixels;
            int pitch;
            SDL_LockTexture(screen_textures[cur_tex], NULL, &pixels, &pitch);

            if (BPP == 8) {
                convert_8bpp_to_rgba(vram, (uint8_t*)pixels, W, H);
            } else if (BPP == 16) {
                convert_16bpp_to_rgba(vram, (uint8_t*)pixels, W, H);
            } else {
                convert_32bpp_to_rgba(vram, (uint8_t*)pixels, W, H);
            }

            SDL_UnlockTexture(screen_textures[cur_tex]);

            // Calculate aspect-ratio-correct destination rectangle
            int win_w, win_h;
            SDL_GetWindowSize(window, &win_w, &win_h);
            
            float aspect_rom = (float)W / (float)H;
            float aspect_win = (float)win_w / (float)win_h;
            
            SDL_Rect dst;
            if (aspect_win > aspect_rom) {
                // Window is wider than ROM - fit height, letterbox sides
                dst.h = win_h;
                dst.w = (int)(win_h * aspect_rom);
                dst.x = (win_w - dst.w) / 2;
                dst.y = 0;
            } else {
                // Window is taller than ROM - fit width, letterbox top/bottom
                dst.w = win_w;
                dst.h = (int)(win_w / aspect_rom);
                dst.x = 0;
                dst.y = (win_h - dst.h) / 2;
            }

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, screen_textures[cur_tex], NULL, &dst);
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(1);
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    for (int i = 0; i < 3; i++) {
        if (screen_textures[i]) SDL_DestroyTexture(screen_textures[i]);
    }
    if (convert_surface) SDL_FreeSurface(convert_surface);
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
