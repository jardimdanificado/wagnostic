#include "gifnostic.h"
#include "gif_encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

struct WagnosticContext {
    IM3Environment env;
    IM3Runtime runtime;
    IM3Module module;
    IM3Function f_wupdate;

    uint8_t* wasm_data;
    size_t wasm_size;

    uint8_t* mem;
    uint32_t mem_len;
    uint32_t state_ptr;

    uint8_t keys[256];
    int32_t mouse_x;
    int32_t mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint32_t gamepad_buttons;
    uint32_t ticks;
    int32_t session_unique;
    uint64_t frame_count;
};

static int32_t generate_unique_id(void) {
    uint64_t t = (uint64_t)time(NULL);
    uint64_t pc = 0;
#if defined(_WIN32)
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    pc = (uint64_t)count.QuadPart;
    uint32_t pid = (uint32_t)GetCurrentProcessId();
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    pc = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    uint32_t pid = (uint32_t)getpid();
#else
    pc = (uint64_t)clock();
    uint32_t pid = 1234;
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
            if (header[124 + i] >= '0' && header[124 + i] <= '7')
                size = size * 8 + (header[124 + i] - '0');
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

static void refresh_memory(WagnosticContext* ctx) {
    if (!ctx || !ctx->runtime) return;
    ctx->mem = m3_GetMemory(ctx->runtime, &ctx->mem_len, 0);
}

static uint32_t compute_bpp(WagnosticState* s) {
    if (!s) return 32;
    uint32_t max_bit = 0;
    if (s->r_bits && s->r_shift + s->r_bits > max_bit) max_bit = s->r_shift + s->r_bits;
    if (s->g_bits && s->g_shift + s->g_bits > max_bit) max_bit = s->g_shift + s->g_bits;
    if (s->b_bits && s->b_shift + s->b_bits > max_bit) max_bit = s->b_shift + s->b_bits;
    if (s->a_bits && s->a_shift + s->a_bits > max_bit) max_bit = s->a_shift + s->a_bits;
    if (s->x_bits && s->x_shift + s->x_bits > max_bit) max_bit = s->x_shift + s->x_bits;

    if (max_bit <= 1) return 1;
    if (max_bit <= 2) return 2;
    if (max_bit <= 4) return 4;
    if (max_bit <= 8) return 8;
    if (max_bit <= 16) return 16;
    if (max_bit <= 24) return 24;
    if (max_bit <= 32) return 32;
    return 64;
}

WagnosticContext* wagnostic_create(const uint8_t* wasm_bytes, size_t wasm_size, uint32_t stack_size_bytes) {
    if (!wasm_bytes || wasm_size == 0) return NULL;

    WagnosticContext* ctx = (WagnosticContext*)calloc(1, sizeof(WagnosticContext));
    if (!ctx) return NULL;

    if (stack_size_bytes == 0) {
        stack_size_bytes = 64 * 1024 * 1024;
    }

    ctx->wasm_size = wasm_size;
    ctx->wasm_data = (uint8_t*)malloc(wasm_size);
    if (!ctx->wasm_data) {
        free(ctx);
        return NULL;
    }
    memcpy(ctx->wasm_data, wasm_bytes, wasm_size);

    ctx->env = m3_NewEnvironment();
    if (!ctx->env) {
        wagnostic_destroy(ctx);
        return NULL;
    }

    ctx->runtime = m3_NewRuntime(ctx->env, stack_size_bytes, NULL);
    if (!ctx->runtime) {
        wagnostic_destroy(ctx);
        return NULL;
    }

    M3Result res = m3_ParseModule(ctx->env, &ctx->module, ctx->wasm_data, ctx->wasm_size);
    if (res) {
        fprintf(stderr, "[gifnostic] m3_ParseModule failed: %s\n", res);
        wagnostic_destroy(ctx);
        return NULL;
    }

    res = m3_LoadModule(ctx->runtime, ctx->module);
    if (res) {
        fprintf(stderr, "[gifnostic] m3_LoadModule failed: %s\n", res);
        wagnostic_destroy(ctx);
        return NULL;
    }

    res = m3_FindFunction(&ctx->f_wupdate, ctx->runtime, "wupdate");
    if (res || !ctx->f_wupdate) {
        fprintf(stderr, "[gifnostic] wupdate() export not found in WASM module: %s\n", res ? res : "not found");
        wagnostic_destroy(ctx);
        return NULL;
    }

    refresh_memory(ctx);
    ctx->session_unique = generate_unique_id();

    res = m3_CallV(ctx->f_wupdate);
    if (!res) {
        m3_GetResultsV(ctx->f_wupdate, &ctx->state_ptr);
    }
    refresh_memory(ctx);

    WagnosticState* state = wagnostic_get_state(ctx);
    if (state && state->unique == 0) {
        state->unique = ctx->session_unique;
    }

    return ctx;
}

WagnosticContext* wagnostic_create_from_file(const char* file_path, uint32_t stack_size_bytes) {
    if (!file_path) return NULL;

    size_t sz = 0;
    uint8_t* wasm_data = tar_extract_file(file_path, "main.wasm", &sz);
    if (!wasm_data) {
        FILE* f = fopen(file_path, "rb");
        if (!f) return NULL;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        wasm_data = (uint8_t*)malloc(sz);
        if (!wasm_data) {
            fclose(f);
            return NULL;
        }
        if (fread(wasm_data, 1, sz, f) != sz) {
            free(wasm_data);
            fclose(f);
            return NULL;
        }
        fclose(f);
    }

    WagnosticContext* ctx = wagnostic_create(wasm_data, sz, stack_size_bytes);
    free(wasm_data);
    return ctx;
}

void wagnostic_destroy(WagnosticContext* ctx) {
    if (!ctx) return;
    if (ctx->runtime) m3_FreeRuntime(ctx->runtime);
    if (ctx->env) m3_FreeEnvironment(ctx->env);
    if (ctx->wasm_data) free(ctx->wasm_data);
    free(ctx);
}

int wagnostic_step(WagnosticContext* ctx) {
    if (!ctx || !ctx->f_wupdate) return 0;

    WagnosticState* state = wagnostic_get_state(ctx);
    if (state) {
        memcpy(state->keys, ctx->keys, 256);
        state->mouse_x = ctx->mouse_x;
        state->mouse_y = ctx->mouse_y;
        state->mouse_buttons = ctx->mouse_buttons;
        state->mouse_wheel = ctx->mouse_wheel;
        state->gamepad_buttons = ctx->gamepad_buttons;
        state->ticks = ctx->ticks;
        if (state->unique == 0) state->unique = ctx->session_unique;
    }

    M3Result call_res = m3_CallV(ctx->f_wupdate);
    if (call_res) {
        fprintf(stderr, "[gifnostic] WASM Execution Error in wupdate(): %s\n", call_res);
        return 0;
    }

    uint32_t keep = 0;
    m3_GetResultsV(ctx->f_wupdate, &keep);
    if (!keep) {
        return 0;
    }

    ctx->state_ptr = keep;
    refresh_memory(ctx);
    ctx->frame_count++;

    ctx->mouse_wheel = 0;

    return 1;
}

WagnosticState* wagnostic_get_state(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->state_ptr == 0) return NULL;
    if (ctx->state_ptr + sizeof(WagnosticState) > ctx->mem_len) return NULL;
    return (WagnosticState*)(ctx->mem + ctx->state_ptr);
}

uint8_t* wagnostic_get_vram(WagnosticContext* ctx) {
    WagnosticState* s = wagnostic_get_state(ctx);
    if (!s || s->vram_offset == 0) return NULL;
    if (ctx->state_ptr + s->vram_offset >= ctx->mem_len) return NULL;
    return (uint8_t*)s + s->vram_offset;
}

uint8_t* wagnostic_get_audio_buffer(WagnosticContext* ctx) {
    WagnosticState* s = wagnostic_get_state(ctx);
    if (!s || s->audio_buffer_offset == 0) return NULL;
    if (ctx->state_ptr + s->audio_buffer_offset >= ctx->mem_len) return NULL;
    return (uint8_t*)s + s->audio_buffer_offset;
}

uint8_t* wagnostic_get_wasm_memory(WagnosticContext* ctx, uint32_t* out_len) {
    if (!ctx) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    refresh_memory(ctx);
    if (out_len) *out_len = ctx->mem_len;
    return ctx->mem;
}

void wagnostic_set_key(WagnosticContext* ctx, uint8_t scancode, uint8_t is_down) {
    if (!ctx) return;
    ctx->keys[scancode] = is_down ? 1 : 0;
}

void wagnostic_set_mouse(WagnosticContext* ctx, int32_t x, int32_t y, uint32_t buttons, int32_t wheel) {
    if (!ctx) return;
    ctx->mouse_x = x;
    ctx->mouse_y = y;
    ctx->mouse_buttons = buttons;
    ctx->mouse_wheel += wheel;
}

void wagnostic_set_gamepad(WagnosticContext* ctx, uint32_t buttons) {
    if (!ctx) return;
    ctx->gamepad_buttons = buttons;
}

void wagnostic_set_ticks(WagnosticContext* ctx, uint32_t ticks_ms) {
    if (!ctx) return;
    ctx->ticks = ticks_ms;
}

static void decode_pixel_to_rgb(WagnosticState* s, uint8_t* vram, uint32_t x, uint32_t y, uint32_t W, uint32_t BPP, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b, uint8_t* out_a) {
    size_t idx = y * W + x;
    uint32_t r_b = s ? s->r_bits : 0;
    uint32_t r_s = s ? s->r_shift : 0;
    uint32_t g_b = s ? s->g_bits : 0;
    uint32_t g_s = s ? s->g_shift : 0;
    uint32_t b_b = s ? s->b_bits : 0;
    uint32_t b_s = s ? s->b_shift : 0;
    uint32_t a_b = s ? s->a_bits : 0;
    uint32_t a_s = s ? s->a_shift : 0;

    if (!r_b && !g_b && !b_b && !a_b) {
        if (BPP == 32) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; a_b = 8; a_s = 24; }
        else if (BPP == 24) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; }
        else if (BPP == 16) { r_b = 5; r_s = 11; g_b = 6; g_s = 5; b_b = 5; b_s = 0; }
        else if (BPP == 8)  { r_b = 3; r_s = 5;  g_b = 3; g_s = 2; b_b = 2; b_s = 0; }
        else { a_b = BPP; a_s = 0; }
    }

    uint64_t raw_pixel = 0;
    if (BPP == 32) {
        raw_pixel = ((uint32_t*)vram)[idx];
    } else if (BPP == 24) {
        raw_pixel = vram[idx * 3] | (vram[idx * 3 + 1] << 8) | (vram[idx * 3 + 2] << 16);
    } else if (BPP == 16) {
        raw_pixel = ((uint16_t*)vram)[idx];
    } else if (BPP == 8) {
        raw_pixel = vram[idx];
    } else if (BPP == 4) {
        uint8_t b = vram[idx / 2];
        raw_pixel = (idx % 2 == 0) ? (b >> 4) : (b & 0x0F);
    } else if (BPP == 2) {
        uint8_t b = vram[idx / 4];
        raw_pixel = (b >> (6 - 2 * (idx % 4))) & 0x03;
    } else if (BPP == 1) {
        uint8_t b = vram[idx / 8];
        raw_pixel = (b >> (7 - (idx % 8))) & 0x01;
    }

    uint8_t r = 0, g = 0, b = 0, a = 255;

    if (r_b > 0) {
        uint32_t val = (raw_pixel >> r_s) & ((1U << r_b) - 1);
        r = (val * 255) / ((1U << r_b) - 1);
    }
    if (g_b > 0) {
        uint32_t val = (raw_pixel >> g_s) & ((1U << g_b) - 1);
        g = (val * 255) / ((1U << g_b) - 1);
    }
    if (b_b > 0) {
        uint32_t val = (raw_pixel >> b_s) & ((1U << b_b) - 1);
        b = (val * 255) / ((1U << b_b) - 1);
    }
    if (a_b > 0) {
        uint32_t val = (raw_pixel >> a_s) & ((1U << a_b) - 1);
        a = (val * 255) / ((1U << a_b) - 1);
    } else if (r_b == 0 && g_b == 0 && b_b == 0) {
        uint32_t max_val = (1U << BPP) - 1;
        uint8_t lum = max_val ? (uint8_t)((raw_pixel * 255) / max_val) : 0;
        r = g = b = lum;
    }

    if (out_r) *out_r = r;
    if (out_g) *out_g = g;
    if (out_b) *out_b = b;
    if (out_a) *out_a = a;
}

int wagnostic_render_rgb24(WagnosticContext* ctx, uint8_t* out_rgb_buffer, size_t buffer_size) {
    WagnosticState* s = wagnostic_get_state(ctx);
    uint8_t* vram = wagnostic_get_vram(ctx);
    if (!s || !vram || !out_rgb_buffer) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint32_t BPP = compute_bpp(s);

    size_t required_sz = (size_t)W * H * 3;
    if (buffer_size < required_sz) return 0;

    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            uint8_t r, g, b, a;
            decode_pixel_to_rgb(s, vram, x, y, W, BPP, &r, &g, &b, &a);
            size_t out_idx = (y * W + x) * 3;
            out_rgb_buffer[out_idx + 0] = r;
            out_rgb_buffer[out_idx + 1] = g;
            out_rgb_buffer[out_idx + 2] = b;
        }
    }
    return 1;
}

int wagnostic_render_rgba32(WagnosticContext* ctx, uint8_t* out_rgba_buffer, size_t buffer_size) {
    WagnosticState* s = wagnostic_get_state(ctx);
    uint8_t* vram = wagnostic_get_vram(ctx);
    if (!s || !vram || !out_rgba_buffer) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint32_t BPP = compute_bpp(s);

    size_t required_sz = (size_t)W * H * 4;
    if (buffer_size < required_sz) return 0;

    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            uint8_t r, g, b, a;
            decode_pixel_to_rgb(s, vram, x, y, W, BPP, &r, &g, &b, &a);
            size_t out_idx = (y * W + x) * 4;
            out_rgba_buffer[out_idx + 0] = r;
            out_rgba_buffer[out_idx + 1] = g;
            out_rgba_buffer[out_idx + 2] = b;
            out_rgba_buffer[out_idx + 3] = a;
        }
    }
    return 1;
}

int wagnostic_dump_ppm(WagnosticContext* ctx, const char* out_filename) {
    WagnosticState* s = wagnostic_get_state(ctx);
    if (!s || !out_filename) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;

    size_t rgb_sz = (size_t)W * H * 3;
    uint8_t* rgb_buf = (uint8_t*)malloc(rgb_sz);
    if (!rgb_buf) return 0;

    if (!wagnostic_render_rgb24(ctx, rgb_buf, rgb_sz)) {
        free(rgb_buf);
        return 0;
    }

    FILE* f = fopen(out_filename, "wb");
    if (!f) {
        free(rgb_buf);
        return 0;
    }

    fprintf(f, "P6\n%u %u\n255\n", W, H);
    fwrite(rgb_buf, 1, rgb_sz, f);
    fclose(f);
    free(rgb_buf);

    return 1;
}

int wagnostic_record_gif(WagnosticContext* ctx, const char* out_filename, uint32_t total_frames, uint32_t frame_skip, uint16_t delay_cs) {
    WagnosticState* s = wagnostic_get_state(ctx);
    if (!s || !out_filename) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;

    size_t rgb_sz = (size_t)W * H * 3;
    uint8_t* rgb_buf = (uint8_t*)malloc(rgb_sz);
    if (!rgb_buf) return 0;

    int loop_count = (total_frames == 1) ? -1 : 0;
    GIFEncoder* gif = gif_create(out_filename, (uint16_t)W, (uint16_t)H, loop_count);
    if (!gif) {
        free(rgb_buf);
        return 0;
    }

    if (delay_cs == 0) delay_cs = 2;

    uint32_t captured_frames = 0;
    for (uint32_t f = 0; total_frames == 0 || f < total_frames; f++) {
        uint32_t current_ticks = (uint32_t)(f * 1000 / 60);
        wagnostic_set_ticks(ctx, current_ticks);

        if (!wagnostic_step(ctx)) break;

        if (f % (frame_skip + 1) == 0) {
            if (wagnostic_render_rgb24(ctx, rgb_buf, rgb_sz)) {
                gif_add_frame(gif, rgb_buf, delay_cs);
                captured_frames++;
            }
        }
    }

    gif_close(gif);
    free(rgb_buf);

    return (captured_frames > 0) ? 1 : 0;
}

void wagnostic_print_debug(WagnosticContext* ctx, FILE* stream) {
    if (!stream) stream = stdout;
    if (!ctx) {
        fprintf(stream, "[gifnostic] Context: NULL\n");
        return;
    }

    WagnosticState* s = wagnostic_get_state(ctx);
    fprintf(stream, "=== Gifnostic Headless Debug Info ===\n");
    fprintf(stream, "Frame Count:   %llu\n", (unsigned long long)ctx->frame_count);
    fprintf(stream, "State Ptr:     0x%08X\n", ctx->state_ptr);
    fprintf(stream, "Linear Memory: %u bytes\n", ctx->mem_len);

    if (!s) {
        fprintf(stream, "State Struct: NULL (Invalid or out of bounds)\n");
        return;
    }

    uint32_t BPP = compute_bpp(s);
    fprintf(stream, "Screen Config: %ux%u (Scale: %u, BPP: %u)\n",
            s->width, s->height, s->scale, BPP);
    fprintf(stream, "Title:         '%s'\n", s->title);
    fprintf(stream, "Offsets:       VRAM=0x%08X, Audio=0x%08X\n",
            s->vram_offset, s->audio_buffer_offset);
    fprintf(stream, "Bitfield Config: R(%u<<%u) G(%u<<%u) B(%u<<%u) A(%u<<%u) X(%u<<%u)\n",
            s->r_bits, s->r_shift, s->g_bits, s->g_shift,
            s->b_bits, s->b_shift, s->a_bits, s->a_shift,
            s->x_bits, s->x_shift);
    fprintf(stream, "Input State:   Mouse(%d, %d, btns=0x%X, wheel=%d) Gamepad=0x%X\n",
            s->mouse_x, s->mouse_y, s->mouse_buttons, s->mouse_wheel, s->gamepad_buttons);
    fprintf(stream, "Audio Config:  Size=%u, Rate=%u, BPP=%u, Channels=%u, Volume=%u, Paused=%u\n",
            s->audio_size, s->audio_sample_rate, s->audio_bpp, s->audio_channels, s->audio_volume, s->audio_paused);
    fprintf(stream, "Audio Buffer:  Write=%u, Read=%u (Underrun=%u, Overrun=%u)\n",
            s->audio_write, s->audio_read, s->audio_underrun, s->audio_overrun);
    fprintf(stream, "Unique ID:     0x%08X\n", s->unique);
    fprintf(stream, "=====================================\n");
}
