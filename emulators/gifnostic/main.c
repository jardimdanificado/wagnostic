#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gifnostic.h"
#include "gif_encoder.h"

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <rom.wasm | rom.wag | rom.tar>\n", prog);
    printf("Gifnostic - Pure WASM3 Headless Wagnostic Emulator & GIF Exporter\n\n");
    printf("Options:\n");
    printf("  -n <frames>       Run for N frames (0 = run until ROM quits, default: 60)\n");
    printf("  -g, --gif <file>  Save output as GIF (static image if N=1, animated GIF if N>1)\n");
    printf("  --delay <cs>      GIF frame delay in centiseconds (1/100s, default: 2 = 20ms)\n");
    printf("  --fps <fps>       GIF frame rate in FPS (e.g. 50, 30, 60)\n");
    printf("  --skip <count>    Skip N frames between captured GIF frames (default: 0)\n");
    printf("  -o <file.ppm>     Save final frame VRAM to PPM image\n");
    printf("  -d                Print debug state info\n");
    printf("  -b                Benchmark mode (measures execution FPS)\n");
    printf("  -h, --help        Show this help message\n");
}

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* rom_path = NULL;
    const char* ppm_path = NULL;
    const char* gif_path = NULL;
    uint64_t max_frames = 60;
    uint32_t frame_skip = 0;
    uint16_t delay_cs = 2; /* 20ms = ~50 FPS default */
    int debug_flag = 0;
    int benchmark_flag = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_frames = strtoull(argv[++i], NULL, 10);
        } else if ((strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gif") == 0) && i + 1 < argc) {
            gif_path = argv[++i];
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            delay_cs = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            int fps = atoi(argv[++i]);
            if (fps > 0) delay_cs = (uint16_t)(100 / fps);
            if (delay_cs == 0) delay_cs = 1;
        } else if (strcmp(argv[i], "--skip") == 0 && i + 1 < argc) {
            frame_skip = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            debug_flag = 1;
        } else if (strcmp(argv[i], "-b") == 0) {
            benchmark_flag = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            ppm_path = argv[++i];
        } else if (argv[i][0] != '-') {
            rom_path = argv[i];
        }
    }

    if (!rom_path) {
        fprintf(stderr, "Error: No ROM file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    WagnosticContext* ctx = wagnostic_create_from_file(rom_path, 0);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to initialize Wagnostic context for '%s'\n", rom_path);
        return 1;
    }

    if (debug_flag) {
        printf("--- Initial State ---\n");
        wagnostic_print_debug(ctx, stdout);
    }

    if (gif_path) {
        printf("Gifnostic: Recording GIF (%s, %llu frames max, skip: %u, delay: %ucs)...\n",
               (max_frames == 1) ? "static image" : "animated GIF",
               (unsigned long long)max_frames, frame_skip, delay_cs);

        if (wagnostic_record_gif(ctx, gif_path, (uint32_t)max_frames, frame_skip, delay_cs)) {
            printf("Gifnostic: Successfully saved GIF to '%s'\n", gif_path);
        } else {
            fprintf(stderr, "Gifnostic: Failed to save GIF to '%s'\n", gif_path);
        }

        if (debug_flag) {
            printf("--- Final State ---\n");
            wagnostic_print_debug(ctx, stdout);
        }

        wagnostic_destroy(ctx);
        return 0;
    }

    double start_time = get_time_sec();
    uint64_t frames_run = 0;

    while (max_frames == 0 || frames_run < max_frames) {
        uint32_t current_ticks = (uint32_t)(frames_run * 1000 / 60);
        wagnostic_set_ticks(ctx, current_ticks);

        int ok = wagnostic_step(ctx);
        if (!ok) {
            printf("ROM requested exit or error occurred at frame %llu.\n", (unsigned long long)frames_run);
            break;
        }
        frames_run++;
    }

    double elapsed_time = get_time_sec() - start_time;

    if (benchmark_flag) {
        double fps = (elapsed_time > 0.0) ? ((double)frames_run / elapsed_time) : 0.0;
        printf("=== Gifnostic Benchmark Results ===\n");
        printf("ROM:            %s\n", rom_path);
        printf("Frames Executed: %llu\n", (unsigned long long)frames_run);
        printf("Total Time:     %.4f seconds\n", elapsed_time);
        printf("Execution Rate: %.2f FPS (frames/sec)\n", fps);
        printf("===================================\n");
    } else {
        printf("Gifnostic: Executed %llu frames in %.4f seconds.\n", (unsigned long long)frames_run, elapsed_time);
    }

    if (ppm_path) {
        if (wagnostic_dump_ppm(ctx, ppm_path)) {
            printf("Gifnostic: Saved final frame VRAM to '%s'\n", ppm_path);
        } else {
            fprintf(stderr, "Gifnostic: Failed to save frame to '%s'\n", ppm_path);
        }
    }

    if (debug_flag) {
        printf("--- Final State ---\n");
        wagnostic_print_debug(ctx, stdout);
    }

    wagnostic_destroy(ctx);
    return 0;
}
