#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

// strlen for wasm3
m3ApiRawFunction(m3_libc_strlen) {
    m3ApiReturnType(int32_t)
    m3ApiGetArgMem(const char*, str)
    int32_t len = 0;
    while (str[len] != 0) len++;
    m3ApiReturn(len);
}

static IM3Module g_module = NULL;
static IM3Runtime g_runtime = NULL;

// ============================================
// Global read/write helpers
// ============================================

static uint32_t read_u32(const char* name) {
    IM3Global global = m3_FindGlobal(g_module, name);
    if (!global) return 0;
    M3TaggedValue value;
    if (m3_GetGlobal(global, &value)) return 0;
    uint32_t ptr = value.value.i32;
    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    if (!mem || ptr == 0) return 0;
    return *(uint32_t*)(mem + ptr);
}

static void write_u32(const char* name, uint32_t val) {
    IM3Global global = m3_FindGlobal(g_module, name);
    if (!global) return;
    M3TaggedValue value;
    if (m3_GetGlobal(global, &value)) return;
    uint32_t ptr = value.value.i32;
    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    if (!mem || ptr == 0) return;
    *(uint32_t*)(mem + ptr) = val;
}

static int32_t read_i32(const char* name) {
    return (int32_t)read_u32(name);
}

static void write_i32(const char* name, int32_t val) {
    write_u32(name, (uint32_t)val);
}

static uint8_t read_u8(const char* name) {
    IM3Global global = m3_FindGlobal(g_module, name);
    if (!global) return 0;
    M3TaggedValue value;
    if (m3_GetGlobal(global, &value)) return 0;
    uint32_t ptr = value.value.i32;
    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    if (!mem || ptr == 0) return 0;
    return mem[ptr];
}

static void write_u8(const char* name, uint8_t val) {
    IM3Global global = m3_FindGlobal(g_module, name);
    if (!global) return;
    M3TaggedValue value;
    if (m3_GetGlobal(global, &value)) return;
    uint32_t ptr = value.value.i32;
    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    if (!mem || ptr == 0) return;
    mem[ptr] = val;
}

static void read_str(const char* name, char* dst, int max) {
    IM3Global global = m3_FindGlobal(g_module, name);
    if (!global) { dst[0] = '\0'; return; }
    M3TaggedValue value;
    if (m3_GetGlobal(global, &value)) { dst[0] = '\0'; return; }
    uint32_t ptr = value.value.i32;
    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    if (!mem || ptr == 0) { dst[0] = '\0'; return; }
    strncpy(dst, (char*)(mem + ptr), max - 1);
    dst[max - 1] = '\0';
}

// ============================================
// Audio callback
// ============================================

static void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    float* stream = (float*)stream_ptr;
    int ns = len_bytes / sizeof(float);

    uint32_t r = read_u32("w_audio_read");
    uint32_t w = read_u32("w_audio_write");
    uint32_t size = read_u32("w_audio_size");
    uint32_t bpp = read_u32("w_audio_bpp");

    if (size == 0) return;

    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    IM3Global buf_global = m3_FindGlobal(g_module, "w_audio_buffer");
    if (!buf_global || !mem) return;
    M3TaggedValue buf_val;
    if (m3_GetGlobal(buf_global, &buf_val)) return;
    uint32_t buf_ptr = buf_val.value.i32;
    if (buf_ptr == 0) return;
    uint8_t* audio_buf = mem + buf_ptr;

    for (int i = 0; i < ns; i++) {
        if (r == w) { stream[i] = 0.0f; continue; }
        float sample = 0;
        if (bpp == 1) {
            sample = (audio_buf[r] - 128) / 128.0f;
            r = (r + 1) % size;
        } else if (bpp == 2) {
            sample = (*(int16_t*)(audio_buf + r)) / 32768.0f;
            r = (r + 2) % size;
        } else if (bpp == 4) {
            sample = (*(float*)(audio_buf + r));
            r = (r + 4) % size;
        }
        stream[i] = sample;
    }
    write_u32("w_audio_read", r);
}

// ============================================
// Mouse coordinate conversion
// ============================================

static void convert_mouse_coords(int window_x, int window_y, int* rom_x, int* rom_y, SDL_Window* window) {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    uint32_t W = read_u32("w_width");
    uint32_t H = read_u32("w_height");
    uint32_t SCALE = read_u32("w_scale");
    if (W == 0) W = 320; if (H == 0) H = 240; if (SCALE == 0) SCALE = 1;

    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;

    int dst_x, dst_y, dst_w, dst_h;
    if (aspect_win > aspect_rom) {
        dst_h = win_h;
        dst_w = (int)(win_h * aspect_rom);
        dst_x = (win_w - dst_w) / 2;
        dst_y = 0;
    } else {
        dst_w = win_w;
        dst_h = (int)(win_w / aspect_rom);
        dst_x = 0;
        dst_y = (win_h - dst_h) / 2;
    }

    float scale_x = (float)W / dst_w;
    float scale_y = (float)H / dst_h;

    *rom_x = (int)((window_x - dst_x) * scale_x);
    *rom_y = (int)((window_y - dst_y) * scale_y);

    if (*rom_x < 0) *rom_x = 0;
    if (*rom_x >= (int)W) *rom_x = W - 1;
    if (*rom_y < 0) *rom_y = 0;
    if (*rom_y >= (int)H) *rom_y = H - 1;
}

// ============================================
// Main
// ============================================

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
    g_runtime = m3_NewRuntime(env, 64*1024*1024, NULL);
    m3_ParseModule(env, &g_module, wasm_data, sz);
    m3_LoadModule(g_runtime, g_module);
    m3_LinkLibC(g_module);

    // Link strlen
    {
        static const char* env_name = "env";
        M3Result lr = m3_LinkRawFunction(g_module, env_name, "strlen", "i(i)", &m3_libc_strlen);
        if (lr) fprintf(stderr, "Warning: strlen link failed: %s\n", lr);
    }

    IM3Function f_init = NULL, f_upd = NULL;
    m3_FindFunction(&f_init, g_runtime, "winit");
    m3_FindFunction(&f_upd, g_runtime, "wupdate");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Call winit
    if (f_init) m3_CallV(f_init);

    // Read config from globals
    uint32_t W = read_u32("w_width");
    uint32_t H = read_u32("w_height");
    uint32_t BPP = read_u32("w_bpp");
    uint32_t SCALE = read_u32("w_scale");
    if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 16; if (SCALE == 0) SCALE = 1;

    char title[128];
    read_str("w_title", title, 128);

    SDL_Window* window = SDL_CreateWindow(title[0] ? title : "Wagnostic",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W * SCALE, H * SCALE, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) { fprintf(stderr, "Window failed: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING, W, H);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    SDL_AudioDeviceID audio_dev = 0;
    uint32_t audio_size = read_u32("w_audio_size");
    if (audio_size > 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = read_u32("w_audio_sample_rate");
        if (wanted.freq == 0) wanted.freq = 44100;
        wanted.format = AUDIO_F32;
        wanted.channels = read_u32("w_audio_channels");
        if (wanted.channels == 0) wanted.channels = 1;
        wanted.samples = 1024;
        wanted.callback = host_audio_callback;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }

    int running = 1;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (ev.key.keysym.scancode < 256)
                    write_u8("w_keys", ev.key.keysym.scancode);  // This writes to offset 0 of keys array
                    // Need to write to keys[scancode] specifically
                    IM3Global keys_global = m3_FindGlobal(g_module, "w_keys");
                    if (keys_global) {
                        M3TaggedValue kv;
                        if (!m3_GetGlobal(keys_global, &kv)) {
                            uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
                            if (mem && kv.value.i32) {
                                mem[kv.value.i32 + ev.key.keysym.scancode] = (ev.type == SDL_KEYDOWN) ? 1 : 0;
                            }
                        }
                    }
            }
            if (ev.type == SDL_MOUSEMOTION) {
                int rx, ry;
                convert_mouse_coords(ev.motion.x, ev.motion.y, &rx, &ry, window);
                write_i32("w_mouse_x", rx);
                write_i32("w_mouse_y", ry);
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                uint32_t buttons = read_u32("w_mouse_buttons");
                if (ev.button.button == SDL_BUTTON_LEFT)
                    buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (buttons | 1) : (buttons & ~1);
                if (ev.button.button == SDL_BUTTON_RIGHT)
                    buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (buttons | 2) : (buttons & ~2);
                write_u32("w_mouse_buttons", buttons);
            }
            if (ev.type == SDL_MOUSEWHEEL) {
                write_i32("w_mouse_wheel", read_i32("w_mouse_wheel") + ev.wheel.y);
            }
        }

        // Update ticks
        write_u32("w_ticks", SDL_GetTicks());

        // Call wupdate
        if (f_upd) m3_CallV(f_upd);

        // Read config again (ROM may have changed it)
        W = read_u32("w_width");
        H = read_u32("w_height");
        BPP = read_u32("w_bpp");
        SCALE = read_u32("w_scale");
        if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 16; if (SCALE == 0) SCALE = 1;

        // Check signals
        uint8_t redraw = read_u8("w_signal_redraw");
        uint8_t quit = read_u8("w_signal_quit");
        uint8_t update_window = read_u8("w_signal_update_window");
        uint8_t update_audio = read_u8("w_signal_update_audio");

        if (quit) running = 0;

        if (update_window || update_audio) {
            // Re-read title
            read_str("w_title", title, 128);
            SDL_SetWindowSize(window, W * SCALE, H * SCALE);
            SDL_SetWindowTitle(window, title[0] ? title : "Wagnostic");

            // Reinit texture
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                SDL_TEXTUREACCESS_STREAMING, W, H);
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

            // Reinit audio if needed
            if (update_audio && audio_size > 0 && audio_dev == 0) {
                SDL_AudioSpec wanted;
                SDL_zero(wanted);
                wanted.freq = read_u32("w_audio_sample_rate");
                if (wanted.freq == 0) wanted.freq = 44100;
                wanted.format = AUDIO_F32;
                wanted.channels = read_u32("w_audio_channels");
                if (wanted.channels == 0) wanted.channels = 1;
                wanted.samples = 1024;
                wanted.callback = host_audio_callback;
                audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
                if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
            }

            write_u8("w_signal_update_window", 0);
            write_u8("w_signal_update_audio", 0);
        }

        if (redraw) {
            // Get VRAM pointer
            IM3Global vram_global = m3_FindGlobal(g_module, "w_vram");
            if (vram_global) {
                M3TaggedValue value;
                if (!m3_GetGlobal(vram_global, &value)) {
                    uint32_t vram_ptr = value.value.i32;
                    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
                    if (mem && vram_ptr) {
                        // Convert to ABGR for SDL texture
                        void* pixels;
                        int pitch;
                        SDL_LockTexture(texture, NULL, &pixels, &pitch);

                        if (BPP == 16) {
                            uint16_t* src = (uint16_t*)(mem + vram_ptr);
                            uint32_t* dst = (uint32_t*)pixels;
                            for (int i = 0; i < (int)(W * H); i++) {
                                uint16_t p = src[i];
                                uint32_t r = ((p >> 11) & 0x1F) * 255 / 31;
                                uint32_t g = ((p >> 5) & 0x3F) * 255 / 63;
                                uint32_t b = (p & 0x1F) * 255 / 31;
                                dst[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
                            }
                        } else if (BPP == 32) {
                            memcpy(pixels, mem + vram_ptr, W * H * 4);
                        } else if (BPP == 8) {
                            uint8_t* src = mem + vram_ptr;
                            uint32_t* dst = (uint32_t*)pixels;
                            for (int i = 0; i < (int)(W * H); i++) {
                                uint8_t p = src[i];
                                uint32_t r = ((p >> 5) & 0x07) * 255 / 7;
                                uint32_t g = ((p >> 2) & 0x07) * 255 / 7;
                                uint32_t b = (p & 0x03) * 255 / 3;
                                dst[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
                            }
                        }

                        SDL_UnlockTexture(texture);
                    }
                }
            }

            // Calculate aspect-ratio-correct destination
            int win_w, win_h;
            SDL_GetWindowSize(window, &win_w, &win_h);
            float aspect_rom = (float)W / (float)H;
            float aspect_win = (float)win_w / (float)win_h;
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

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &dst);
            SDL_RenderPresent(renderer);

            write_u8("w_signal_redraw", 0);
        }

        // Reset mouse wheel
        write_i32("w_mouse_wheel", 0);

        SDL_Delay(1);
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}