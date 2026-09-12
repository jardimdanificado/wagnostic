/*
 * Wagnostic 2.0 Native Runner — 100% Libc/POSIX Terminal & GIF Host
 *
 * Implements the Wagnostic 2.0 ABI using wasm3:
 * - Exports: wupdate() -> int32_t (WUPDATE_OK, WUPDATE_EXIT, WUPDATE_ERROR)
 * - Imports: env.wextension(const char *name, uint32_t version) -> void*
 *
 * Standard Extensions:
 * - std:framebuffer (v1) — Terminal ANSI TrueColor (▀) rendering
 * - std:clock       (v1) — Monotonic clock and frame delta
 * - std:io          (v1) — Unified input (Keyboard scancodes, Gamepad, Mouse)
 * - std:gif         (v1) — Headless GIF recording synchronization
 * - logger          (v1) — UTF-8 debug logging to host terminal
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

#include "wagnostic.h"
#include "gif_encoder.h"

/* Standard Extension Structures & Constants (as documented in STD.md) */
#define WFRAMEBUFFER_EXTENSION "std:framebuffer"
#define WCLOCK_EXTENSION       "std:clock"
#define WKEYBOARD_EXTENSION    "std:keyboard"
#define WMOUSE_EXTENSION       "std:mouse"
#define WLOGGER_EXTENSION      "logger"

#define WMOUSE_BTN_LEFT        (1 << 0)
#define WMOUSE_BTN_RIGHT       (1 << 1)
#define WMOUSE_BTN_MIDDLE      (1 << 2)

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixels;
} wframebuffer_t;

typedef struct {
    uint64_t ticks;
    uint64_t frequency;
    float    delta;
} wclock_t;

typedef struct {
    uint8_t keys[256];
} wkeyboard_t;

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t buttons;
    int32_t  wheel_x;
    int32_t  wheel_y;
} wmouse_t;

typedef struct {
    uint32_t buffer;
    uint32_t capacity;
    uint32_t length;
} wlogger_t;

/* ================================================================
 * Globals & State
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

static uint8_t *g_mem     = NULL;
static size_t   g_mem_len = 0;

static uint32_t g_fb_ptr       = 0;
static uint32_t g_clock_ptr    = 0;
static uint32_t g_keyboard_ptr = 0;
static uint32_t g_mouse_ptr    = 0;
static uint32_t g_logger_ptr   = 0;

static uint32_t g_default_fb_ptr = 0;
static uint32_t g_logger_buf_ptr = 0;
static uint32_t g_arena_offset   = 0;

static const char *g_gif_path    = NULL;
static GIFEncoder *g_gif_encoder = NULL;
static uint8_t    *g_gif_rgb_buf = NULL;
static uint64_t    g_max_frames  = 0;
static uint32_t    g_target_fps  = 30;
static char        g_rom_path[1024] = {0};

/* ================================================================
 * Memory & Arena Helpers
 * ================================================================ */

static void refresh_memory(void) {
    if (g_module) {
        g_mem = m3_GetMemory(g_module, &g_mem_len, 0);
    }
}

static uint32_t host_alloc(uint32_t size, uint32_t align) {
    refresh_memory();
    if (g_arena_offset == 0) {
        if (g_module) {
            for (uint32_t i = 0; i < g_module->numGlobals; i++) {
                if (g_module->globals[i].name && strcmp(g_module->globals[i].name, "__heap_base") == 0) {
                    g_arena_offset = (uint32_t)g_module->globals[i].i32Value;
                    break;
                }
            }
        }
        if (g_arena_offset == 0) {
            g_arena_offset = (g_mem_len > 1048576) ? 0x20000 : 0x8000;
        }
    }
    if (align > 1) {
        g_arena_offset = (g_arena_offset + align - 1) & ~(align - 1);
    }
    uint32_t ptr = g_arena_offset;
    g_arena_offset += size;
    if (g_arena_offset > g_mem_len && g_runtime && g_module && g_module->numMemories > 0) {
        uint32_t pages = (g_arena_offset + 65535) / 65536;
        ResizeMemory(g_runtime, g_module->memories[0], pages);
        refresh_memory();
    }
    return ptr;
}

/* ================================================================
 * Extension Dispatcher (env.wextension)
 * ================================================================ */

m3ApiRawFunction(host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);

    refresh_memory();
    if (!g_mem || name_ptr >= g_mem_len) m3ApiReturn(0);

    const char *name = (const char*)(g_mem + name_ptr);

    /* 1. Framebuffer: std:framebuffer */
    if (strcmp(name, WFRAMEBUFFER_EXTENSION) == 0 || strcmp(name, "surface") == 0 ||
        strcmp(name, "framebuffer") == 0 || strcmp(name, "std:surface") == 0) {
        if (g_fb_ptr == 0) {
            g_fb_ptr = host_alloc(sizeof(wframebuffer_t), 4);
            g_default_fb_ptr = host_alloc(640 * 480 * 4, 4);
            wframebuffer_t *fb = (wframebuffer_t*)(g_mem + g_fb_ptr);
            fb->width = 320;
            fb->height = 240;
            fb->pixels = g_default_fb_ptr;
        }
        m3ApiReturn(g_fb_ptr);
    }

    /* 2. Clock: std:clock */
    if (strcmp(name, WCLOCK_EXTENSION) == 0 || strcmp(name, "clock") == 0) {
        if (g_clock_ptr == 0) {
            g_clock_ptr = host_alloc(sizeof(wclock_t), 8);
            wclock_t *clk = (wclock_t*)(g_mem + g_clock_ptr);
            clk->ticks = 0;
            clk->frequency = 1000;
            clk->delta = 1.0f / (float)g_target_fps;
        }
        m3ApiReturn(g_clock_ptr);
    }

    /* 3. Keyboard: std:keyboard */
    if (strcmp(name, WKEYBOARD_EXTENSION) == 0 || strcmp(name, "keyboard") == 0) {
        if (g_keyboard_ptr == 0) {
            g_keyboard_ptr = host_alloc(sizeof(wkeyboard_t), 4);
            wkeyboard_t *kb = (wkeyboard_t*)(g_mem + g_keyboard_ptr);
            memset(kb, 0, sizeof(wkeyboard_t));
        }
        m3ApiReturn(g_keyboard_ptr);
    }

    /* 4. Mouse: std:mouse */
    if (strcmp(name, WMOUSE_EXTENSION) == 0 || strcmp(name, "mouse") == 0) {
        if (g_mouse_ptr == 0) {
            g_mouse_ptr = host_alloc(sizeof(wmouse_t), 4);
            wmouse_t *mouse = (wmouse_t*)(g_mem + g_mouse_ptr);
            memset(mouse, 0, sizeof(wmouse_t));
        }
        m3ApiReturn(g_mouse_ptr);
    }

    /* 5. Logger: logger */
    if (strcmp(name, "logger") == 0) {
        if (g_logger_ptr == 0) {
            g_logger_ptr = host_alloc(sizeof(wlogger_t), 4);
            g_logger_buf_ptr = host_alloc(1024, 4);
            wlogger_t *log = (wlogger_t*)(g_mem + g_logger_ptr);
            log->buffer = g_logger_buf_ptr;
            log->capacity = 1024;
            log->length = 0;
        }
        m3ApiReturn(g_logger_ptr);
    }

    m3ApiReturn(0);
}

/* ================================================================
 * Cleanup & Frame Processing (GIF)
 * ================================================================ */

static void cleanup(void) {
    if (g_gif_encoder && g_gif_path) {
        gif_close(g_gif_encoder);
        g_gif_encoder = NULL;
        printf("[GIF] Saved animation to %s\n", g_gif_path);
    }
    if (g_gif_rgb_buf) {
        free(g_gif_rgb_buf);
        g_gif_rgb_buf = NULL;
    }
}

static void process_gif_frame(wframebuffer_t *fb) {
    if (!fb || fb->pixels == 0 || !g_gif_path) return;
    refresh_memory();
    if (!g_mem || fb->pixels >= g_mem_len) return;

    uint32_t W = fb->width ? fb->width : 320;
    uint32_t H = fb->height ? fb->height : 240;
    uint32_t *vram = (uint32_t*)(g_mem + fb->pixels);

    if (!g_gif_encoder) {
        g_gif_encoder = gif_create(g_gif_path, (uint16_t)W, (uint16_t)H, 0);
        g_gif_rgb_buf = (uint8_t*)malloc(W * H * 3);
    }
    if (g_gif_encoder && g_gif_rgb_buf) {
        for (uint32_t i = 0; i < W * H; i++) {
            uint32_t px = vram[i];
            g_gif_rgb_buf[i * 3 + 0] = px & 0xFF;
            g_gif_rgb_buf[i * 3 + 1] = (px >> 8) & 0xFF;
            g_gif_rgb_buf[i * 3 + 2] = (px >> 16) & 0xFF;
        }
        gif_add_frame(g_gif_encoder, g_gif_rgb_buf, (uint16_t)(100 / (g_target_fps ? g_target_fps : 30)));
    }
}

/* ================================================================
 * Main Host Entry Point
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Wagnostic 2.0 Native CLI Runner & GIF Exporter\n");
        printf("Usage: %s <rom.wasm> [-o <out.gif>] [-n <frames>] [-fps <fps>]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            g_max_frames = strtoull(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-n=", 3) == 0) {
            g_max_frames = strtoull(argv[i] + 3, NULL, 10);
        } else if (strcmp(argv[i], "-fps") == 0 && i + 1 < argc) {
            g_target_fps = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "--fps=", 6) == 0) {
            g_target_fps = (uint32_t)strtoul(argv[i] + 6, NULL, 10);
        } else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-g") == 0) && i + 1 < argc) {
            g_gif_path = argv[++i];
        } else if (strncmp(argv[i], "-o=", 3) == 0 || strncmp(argv[i], "-g=", 3) == 0) {
            g_gif_path = argv[i] + 3;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            g_gif_path = argv[i] + 9;
        } else if (argv[i][0] != '-' && g_rom_path[0] == '\0') {
            strncpy(g_rom_path, argv[i], sizeof(g_rom_path) - 1);
        }
    }

    if (g_rom_path[0] == '\0') {
        fprintf(stderr, "Error: No ROM file specified.\n");
        return 1;
    }

    /* Load WASM binary */
    FILE *f = fopen(g_rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open ROM file: %s\n", g_rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    size_t wasm_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *wasm_bytes = (uint8_t*)malloc(wasm_size);
    if (!wasm_bytes) {
        fprintf(stderr, "Out of memory\n");
        fclose(f);
        return 1;
    }
    fread(wasm_bytes, 1, wasm_size, f);
    fclose(f);

    /* Initialize wasm3 */
    IM3Environment env = m3_NewEnvironment();
    if (!env) {
        fprintf(stderr, "Failed to create wasm3 environment\n");
        return 1;
    }

    g_runtime = m3_NewRuntime(env, 64 * 1024, NULL);
    if (!g_runtime) {
        fprintf(stderr, "Failed to create wasm3 runtime\n");
        return 1;
    }

    M3Result result = m3_ParseModule(env, &g_module, wasm_bytes, (uint32_t)wasm_size);
    if (result) {
        fprintf(stderr, "Failed to parse WASM module: %s\n", result);
        return 1;
    }

    result = m3_LoadModule(g_runtime, g_module);
    if (result) {
        fprintf(stderr, "Failed to load WASM module: %s\n", result);
        return 1;
    }

    /* Link wextension import */
    m3_LinkRawFunction(g_module, "env", "wextension", "i(i)", &host_wextension);

    /* Lookup wupdate export */
    IM3Function f_wupdate = NULL;
    result = m3_FindFunction(&f_wupdate, g_runtime, "wupdate");
    if (result || !f_wupdate) {
        fprintf(stderr, "Error: ROM does not export 'wupdate()' function\n");
        return 1;
    }

    atexit(cleanup);

    uint64_t frame_count = 0;
    struct timespec start_ts, last_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    last_ts = start_ts;

    while (g_max_frames == 0 || frame_count < g_max_frames) {
        frame_count++;

        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed_ms = (now_ts.tv_sec - start_ts.tv_sec) * 1000.0 + (now_ts.tv_nsec - start_ts.tv_nsec) / 1000000.0;
        double dt = (now_ts.tv_sec - last_ts.tv_sec) + (now_ts.tv_nsec - last_ts.tv_nsec) / 1000000000.0;
        last_ts = now_ts;

        /* Update Clock */
        if (g_clock_ptr && g_clock_ptr + sizeof(wclock_t) <= g_mem_len) {
            wclock_t *clk = (wclock_t*)(g_mem + g_clock_ptr);
            clk->ticks = (uint64_t)elapsed_ms;
            clk->delta = (float)dt;
        }

        /* Update Logger */
        if (g_logger_ptr && g_logger_ptr + sizeof(wlogger_t) <= g_mem_len) {
            wlogger_t *log = (wlogger_t*)(g_mem + g_logger_ptr);
            uint32_t len = log->length;
            if (len > 0 && g_logger_buf_ptr && g_logger_buf_ptr + len <= g_mem_len) {
                char log_str[1024];
                uint32_t cpy_len = len < 1023 ? len : 1023;
                memcpy(log_str, g_mem + g_logger_buf_ptr, cpy_len);
                log_str[cpy_len] = '\0';
                printf("[ROM Log] %s\n", log_str);
                log->length = 0;
            }
        }

        /* Call wupdate() */
        result = m3_CallV(f_wupdate);
        if (result) {
            fprintf(stderr, "Runtime error in wupdate(): %s\n", result);
            break;
        }

        int32_t status = 0;
        m3_GetResultsV(f_wupdate, &status);

        if (status == WUPDATE_EXIT) {
            break;
        }
        if (status < 0) {
            fprintf(stderr, "wupdate() returned error code %d\n", status);
            return status;
        }

        /* Capture GIF frame if enabled */
        if (g_fb_ptr && g_fb_ptr + sizeof(wframebuffer_t) <= g_mem_len) {
            wframebuffer_t *fb = (wframebuffer_t*)(g_mem + g_fb_ptr);
            process_gif_frame(fb);
        }
    }

    cleanup();
    return 0;
}
