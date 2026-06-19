#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

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

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rom.wasm> <num_frames>\n", argv[0]);
        return 1;
    }

    const char* wasm_path = argv[1];
    int num_frames = atoi(argv[2]);
    if (num_frames <= 0) num_frames = 100;

    // Load WASM file
    FILE* f = fopen(wasm_path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open %s\n", wasm_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);

    // Create wasm3 runtime with 64MB stack
    IM3Environment env = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024 * 1024, NULL);
    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm_data, sz);
    if (result) {
        fprintf(stderr, "ERROR: Parse failed: %s\n", result);
        return 1;
    }
    result = m3_LoadModule(runtime, module);
    if (result) {
        fprintf(stderr, "ERROR: Load failed: %s\n", result);
        return 1;
    }

    IM3Function f_init = NULL, f_upd = NULL;
    m3_FindFunction(&f_init, runtime, "winit");
    m3_FindFunction(&f_upd, runtime, "wupdate");

    if (!f_init || !f_upd) {
        fprintf(stderr, "ERROR: ROM missing winit or wupdate\n");
        return 1;
    }

    // Call winit
    result = m3_CallV(f_init);
    if (result) {
        fprintf(stderr, "ERROR: winit failed: %s\n", result);
        return 1;
    }

    // Read config after init
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) {
        fprintf(stderr, "ERROR: Cannot access WASM memory\n");
        return 1;
    }
    SystemConfig* sys = (SystemConfig*)mem;

    uint32_t W = sys->width;
    uint32_t H = sys->height;
    uint32_t BPP = sys->bpp;
    uint32_t audio_size = sys->audio_size;
    uint64_t vram_bytes = (uint64_t)W * H * (BPP / 8);

    printf("========================================\n");
    printf("  WAGNOSTIC BENCHMARK\n");
    printf("========================================\n");
    printf("ROM:          %s\n", wasm_path);
    printf("Title:        %s\n", sys->message);
    printf("Resolution:   %ux%u @ %ubpp\n", W, H, BPP);
    printf("VRAM:         %lu KB\n", (unsigned long)(vram_bytes / 1024));
    printf("Audio:        %u Hz, %u-bit, %u ch, %u KB buffer\n",
           sys->audio_rate, sys->audio_bpp * 8, sys->audio_channels,
           audio_size / 1024);
    printf("Frames:       %d\n", num_frames);
    printf("========================================\n");
    printf("Running benchmark...\n");
    fflush(stdout);

    // Benchmark loop
    uint64_t start_ns = now_ns();
    uint64_t total_update_ns = 0;
    uint64_t min_frame_ns = UINT64_MAX;
    uint64_t max_frame_ns = 0;

    for (int i = 0; i < num_frames; i++) {
        // Update ticks (simulate 16ms per frame = ~60fps)
        sys->ticks = (uint32_t)(i * 16);

        uint64_t frame_start = now_ns();
        result = m3_CallV(f_upd);
        uint64_t frame_end = now_ns();

        if (result) {
            fprintf(stderr, "\nERROR: wupdate failed at frame %d: %s\n", i, result);
            return 1;
        }

        // Clear signals (simulate host behavior)
        mem = m3_GetMemory(runtime, NULL, 0);
        sys = (SystemConfig*)mem;
        for (int s = 0; s < 4; s++) sys->signals[s] = 0;
        sys->mouse_wheel = 0;

        uint64_t frame_ns = frame_end - frame_start;
        total_update_ns += frame_ns;
        if (frame_ns < min_frame_ns) min_frame_ns = frame_ns;
        if (frame_ns > max_frame_ns) max_frame_ns = frame_ns;

        // Progress indicator
        if ((i + 1) % 10 == 0 || i == num_frames - 1) {
            printf("\r  Frame %d/%d (%.1f ms/frame avg)",
                   i + 1, num_frames,
                   (double)total_update_ns / (i + 1) / 1e6);
            fflush(stdout);
        }
    }

    uint64_t end_ns = now_ns();
    uint64_t total_ns = end_ns - start_ns;

    double total_ms = (double)total_ns / 1e6;
    double avg_frame_ms = (double)total_update_ns / num_frames / 1e6;
    double min_frame_ms = (double)min_frame_ns / 1e6;
    double max_frame_ms = (double)max_frame_ns / 1e6;
    double fps = 1000.0 / avg_frame_ms;
    double pixels_per_sec = (double)W * H * num_frames / (total_ms / 1000.0);
    double megapixels_per_sec = pixels_per_sec / 1e6;

    printf("\n\n");
    printf("========================================\n");
    printf("  RESULTS\n");
    printf("========================================\n");
    printf("Total time:       %.2f ms\n", total_ms);
    printf("Avg frame time:   %.3f ms\n", avg_frame_ms);
    printf("Min frame time:   %.3f ms\n", min_frame_ms);
    printf("Max frame time:   %.3f ms\n", max_frame_ms);
    printf("Avg FPS:          %.1f\n", fps);
    printf("Pixels/sec:       %.2f MP/s\n", megapixels_per_sec);
    printf("VRAM per frame:   %.2f KB\n", (double)vram_bytes / 1024.0);
    printf("VRAM bandwidth:   %.2f MB/s\n", 
           (double)vram_bytes * num_frames / total_ms * 1000.0 / (1024.0 * 1024.0));
    if (audio_size > 0) {
        double audio_samples = (double)sys->audio_rate * sys->audio_channels * num_frames * avg_frame_ms / 1000.0;
        printf("Audio samples:    %.0f total\n", audio_samples);
    }
    printf("========================================\n");

    // Cleanup
    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
