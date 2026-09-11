/*
 * Wagnostic 2.0 Native Runner — Multi-ROM Concurrent Worker & Rendezvous IPC Host
 *
 * Implements:
 * - Multi-ROM worker execution with 1 OS thread per worker
 * - Rendezvous IPC: wask() and wtell()
 * - Standard Extensions: std:framebuffer, std:clock, std:keyboard, std:mouse, std:gamepad, std:gif, logger
 * - ANSI TrueColor Terminal Rendering & Headless GIF Export
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
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#endif

#include "wagnostic.h"
#include "platform.h"
#include "ipc.h"
#include "worker.h"
#include "gif_encoder.h"

#define MAX_WORKERS 64

static WWorker   *g_workers[MAX_WORKERS];
static int        g_worker_count = 0;

static const char *g_gif_path    = NULL;
static GIFEncoder *g_gif_encoder = NULL;
static uint8_t    *g_gif_rgb_buf = NULL;
static uint64_t    g_max_frames  = 0;
static uint32_t    g_target_fps  = 30;
static int         g_headless    = 0;

#if !defined(_WIN32)
static struct termios g_orig_termios;
static int g_termios_saved = 0;
#endif

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
        if (strcmp(name, target_filename) == 0 || strstr(name, target_filename) != NULL) {
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

/* ================================================================
 * Terminal & Input Management
 * ================================================================ */

static void restore_terminal(void) {
#if !defined(_WIN32)
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
        g_termios_saved = 0;
        printf("\x1b[?25h\x1b[0m\n");
        fflush(stdout);
    }
#endif
}

static void enable_raw_terminal(void) {
#if !defined(_WIN32)
    if (!isatty(STDIN_FILENO)) return;
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        g_termios_saved = 1;
        atexit(restore_terminal);

        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

        printf("\x1b[?25l\x1b[2J");
        fflush(stdout);
    }
#endif
}

static void get_terminal_dimensions(int *out_cols, int *out_rows) {
    *out_cols = 80;
    *out_rows = 24;
#if !defined(_WIN32)
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        if (w.ws_col > 0) *out_cols = w.ws_col;
        if (w.ws_row > 0) *out_rows = w.ws_row;
    }
#endif
}

static int poll_terminal_input(WWorker *w) {
#if !defined(_WIN32)
    if (!w || !w->mem) return 1;
    wkeyboard_t *kb = (w->keyboard_ptr && w->keyboard_ptr + sizeof(wkeyboard_t) <= w->mem_len) ? (wkeyboard_t*)(w->mem + w->keyboard_ptr) : NULL;
    wgamepad_t  *gp = (w->gamepad_ptr  && w->gamepad_ptr + sizeof(wgamepad_t)   <= w->mem_len) ? (wgamepad_t*)(w->mem + w->gamepad_ptr)   : NULL;

    uint8_t buf[16];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == 3 || buf[i] == 27 || buf[i] == 'q') { // Ctrl+C, ESC, 'q'
                return 0;
            }
            if (buf[i] == 0x1b && i + 2 < n && buf[i+1] == '[') {
                if (buf[i+2] == 'A') { if (kb) kb->keys[0x52] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_DPAD_UP; }
                if (buf[i+2] == 'B') { if (kb) kb->keys[0x51] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_DPAD_DOWN; }
                if (buf[i+2] == 'D') { if (kb) kb->keys[0x50] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_DPAD_LEFT; }
                if (buf[i+2] == 'C') { if (kb) kb->keys[0x4F] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_DPAD_RIGHT; }
                i += 2;
            }
            if (buf[i] == 'z' || buf[i] == 'Z') { if (kb) kb->keys[0x1D] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_A; }
            if (buf[i] == 'x' || buf[i] == 'X') { if (kb) kb->keys[0x1B] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_B; }
            if (buf[i] == '\n' || buf[i] == '\r') { if (kb) kb->keys[0x28] = 1; if (gp) gp->buttons |= WGAMEPAD_BTN_START; }
        }
    }
#endif
    return 1;
}

/* ================================================================
 * Terminal Frame Rendering (ANSI TrueColor Half-Blocks)
 * ================================================================ */

static void render_terminal_frame(WWorker *w, uint64_t frame_num) {
    if (!w || !w->mem || !w->fb_ptr || w->fb_ptr + sizeof(wframebuffer_t) > w->mem_len) return;

    wframebuffer_t *fb = (wframebuffer_t*)(w->mem + w->fb_ptr);
    if (!fb || fb->pixels == 0 || fb->pixels >= w->mem_len) return;

    uint32_t W = fb->width ? fb->width : 320;
    uint32_t H = fb->height ? fb->height : 240;
    uint32_t *vram = (uint32_t*)(w->mem + fb->pixels);

    /* GIF Capture if enabled */
    if (g_gif_path) {
        if (!g_gif_encoder) {
            g_gif_encoder = gif_create(g_gif_path, (uint16_t)W, (uint16_t)H, 0);
            g_gif_rgb_buf = (uint8_t*)malloc(W * H * 3);
        }
        if (g_gif_encoder && g_gif_rgb_buf) {
            for (uint32_t i = 0; i < W * H; i++) {
                uint32_t px = vram[i];
                g_gif_rgb_buf[i * 3 + 0] = (uint8_t)(px & 0xFF);
                g_gif_rgb_buf[i * 3 + 1] = (uint8_t)((px >> 8) & 0xFF);
                g_gif_rgb_buf[i * 3 + 2] = (uint8_t)((px >> 16) & 0xFF);
            }
            uint16_t delay_cs = (uint16_t)(100 / g_target_fps);
            gif_add_frame(g_gif_encoder, g_gif_rgb_buf, delay_cs);
        }
    }

    if (g_headless) return;

    int term_cols = 80, term_rows = 24;
    get_terminal_dimensions(&term_cols, &term_rows);
    int max_term_rows = (term_rows > 2) ? term_rows - 2 : 20;

    int term_w = (term_cols < (int)W) ? term_cols : (int)W;
    int term_h = (max_term_rows * 2 < (int)H) ? max_term_rows * 2 : (int)H;

    char *out_buf = (char*)malloc(term_w * term_h * 32 + 256);
    if (!out_buf) return;
    char *p = out_buf;

    p += sprintf(p, "\x1b[H");

    for (int ty = 0; ty < term_h; ty += 2) {
        for (int tx = 0; tx < term_w; tx++) {
            int src_x  = (tx * (int)W) / term_w;
            int src_y1 = (ty * (int)H) / term_h;
            int src_y2 = ((ty + 1) * (int)H) / term_h;
            if (src_y2 >= (int)H) src_y2 = (int)H - 1;

            uint32_t px_top = vram[src_y1 * W + src_x];
            uint32_t px_bot = (ty + 1 < term_h) ? vram[src_y2 * W + src_x] : px_top;

            uint8_t r1 = px_top & 0xFF, g1 = (px_top >> 8) & 0xFF, b1 = (px_top >> 16) & 0xFF;
            uint8_t r2 = px_bot & 0xFF, g2 = (px_bot >> 8) & 0xFF, b2 = (px_bot >> 16) & 0xFF;

            p += sprintf(p, "\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um▀", r1, g1, b1, r2, g2, b2);
        }
        p += sprintf(p, "\x1b[0m\n");
    }

    p += sprintf(p, "\x1b[0m\x1b[90m [Wagnostic 2.0] Frame %lu | %ux%u -> %dx%d (Press 'q' or ESC to exit)\x1b[0m",
                 (unsigned long)frame_num, W, H, term_w, term_h);

    fwrite(out_buf, 1, p - out_buf, stdout);
    fflush(stdout);
    free(out_buf);
}

/* ================================================================
 * Main Host Execution Entry Point
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Wagnostic 2.0 Native Multi-ROM Runner\n");
        printf("Usage: %s <rom1.wasm[:name1]> [rom2.wasm[:name2] ...] [-n <frames>] [-fps <fps>] [--headless] [-g <out.gif>]\n", argv[0]);
        return 1;
    }

    char *rom_specs[MAX_WORKERS];
    int rom_spec_count = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-n") == 0 || strcmp(arg, "--frames") == 0) {
            if (i + 1 < argc) g_max_frames = strtoull(argv[++i], NULL, 10);
        } else if (strncmp(arg, "-n=", 3) == 0) {
            g_max_frames = strtoull(arg + 3, NULL, 10);
        } else if (strcmp(arg, "-fps") == 0) {
            if (i + 1 < argc) g_target_fps = atoi(argv[++i]);
        } else if (strncmp(arg, "-fps=", 5) == 0) {
            g_target_fps = atoi(arg + 5);
        } else if (strcmp(arg, "--headless") == 0) {
            g_headless = 1;
        } else if (strcmp(arg, "-g") == 0 || strncmp(arg, "--gif=", 6) == 0) {
            if (strncmp(arg, "--gif=", 6) == 0) g_gif_path = arg + 6;
            else if (i + 1 < argc) g_gif_path = argv[++i];
        } else if (arg[0] != '-') {
            if (rom_spec_count < MAX_WORKERS) {
                rom_specs[rom_spec_count++] = (char*)arg;
            }
        }
    }

    if (rom_spec_count == 0) {
        fprintf(stderr, "Error: No ROM specified.\n");
        return 1;
    }

    if (g_target_fps == 0) g_target_fps = 30;

    wipc_init();

    /* Load and instantiate all workers */
    for (int i = 0; i < rom_spec_count; i++) {
        char path_buf[512] = {0};
        char name_buf[64] = {0};

        char *spec = rom_specs[i];
        char *colon = strchr(spec, ':');
        if (colon) {
            size_t path_len = (size_t)(colon - spec);
            strncpy(path_buf, spec, path_len);
            path_buf[path_len] = '\0';
            strncpy(name_buf, colon + 1, sizeof(name_buf) - 1);
        } else {
            strncpy(path_buf, spec, sizeof(path_buf) - 1);
            /* Extract basename as default name */
            const char *slash = strrchr(path_buf, '/');
            const char *base = slash ? slash + 1 : path_buf;
            strncpy(name_buf, base, sizeof(name_buf) - 1);
            char *dot = strstr(name_buf, ".wasm");
            if (dot) *dot = '\0';
            char *dot_tar = strstr(name_buf, ".tar");
            if (dot_tar) *dot_tar = '\0';
        }

        uint8_t *wasm_buf = NULL;
        size_t wasm_size = 0;

        if (strstr(path_buf, ".tar") != NULL) {
            wasm_buf = tar_extract_file(path_buf, "main.wasm", &wasm_size);
        }

        if (!wasm_buf) {
            FILE *f = fopen(path_buf, "rb");
            if (!f) {
                fprintf(stderr, "Error: Could not open ROM file '%s'\n", path_buf);
                wipc_cleanup();
                return 1;
            }
            fseek(f, 0, SEEK_END);
            wasm_size = (size_t)ftell(f);
            fseek(f, 0, SEEK_SET);
            wasm_buf = (uint8_t*)malloc(wasm_size);
            fread(wasm_buf, 1, wasm_size, f);
            fclose(f);
        }

        uint32_t worker_id = (uint32_t)(i + 1);
        WWorker *w = worker_create(worker_id, name_buf, path_buf, wasm_buf, wasm_size);
        free(wasm_buf);

        if (!w) {
            fprintf(stderr, "Error: Failed to create worker '%s' from '%s'\n", name_buf, path_buf);
            wipc_cleanup();
            return 1;
        }

        g_workers[g_worker_count++] = w;
    }

    /* Start all workers */
    for (int i = 0; i < g_worker_count; i++) {
        worker_start(g_workers[i]);
    }

    if (!g_headless) {
        enable_raw_terminal();
    }

    /* Main presentation & event loop */
    uint64_t last_rendered_frame = 0;
    WWorker *primary = (g_worker_count > 0) ? g_workers[0] : NULL;

    while (1) {
        /* Check if any worker is still running */
        bool any_running = false;
        for (int i = 0; i < g_worker_count; i++) {
            if (g_workers[i]->running) {
                any_running = true;
                break;
            }
        }

        if (!any_running) {
            break;
        }

        /* Input polling */
        if (!g_headless && primary) {
            if (!poll_terminal_input(primary)) {
                break;
            }
        }

        /* Render terminal frame & capture GIF when a new frame is completed */
        uint64_t cur_worker_frame = primary ? primary->frame_count : 0;
        if (cur_worker_frame > last_rendered_frame) {
            last_rendered_frame = cur_worker_frame;
            if (primary) {
                render_terminal_frame(primary, cur_worker_frame);
            }
        }

        if (g_max_frames > 0 && cur_worker_frame >= (uint64_t)g_max_frames) {
            break;
        }

        if (!g_headless) {
            wthread_sleep_ms(1000 / g_target_fps);
        } else {
            wthread_sleep_ms(1);
        }
    }

    if (!g_headless) {
        restore_terminal();
    }

    /* Shutdown IPC to wake any remaining blocked threads */
    wipc_shutdown();

    /* Stop and join all workers */
    int final_exit_code = 0;
    for (int i = 0; i < g_worker_count; i++) {
        worker_stop(g_workers[i]);
        worker_join(g_workers[i]);
        if (g_workers[i]->exit_code != 0 && final_exit_code == 0) {
            final_exit_code = g_workers[i]->exit_code;
        }
    }

    /* Finalize GIF if active */
    if (g_gif_encoder) {
        gif_close(g_gif_encoder);
        g_gif_encoder = NULL;
        if (g_gif_rgb_buf) { free(g_gif_rgb_buf); g_gif_rgb_buf = NULL; }
        printf("[GIF] Animation saved to %s\n", g_gif_path);
    }

    for (int i = 0; i < g_worker_count; i++) {
        worker_destroy(g_workers[i]);
    }
    g_worker_count = 0;

    wipc_cleanup();
    return (final_exit_code < 0) ? final_exit_code : 0;
}
