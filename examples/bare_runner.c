/**
 * Wagnostic 2.0 — Bare C Runner (Minimal Reference Host)
 *
 * Minimal standalone C host using wasm3.
 * Demonstrates the core Wagnostic contract:
 *   - Host provides `wextension(name, version)`
 *   - Guest exports `wupdate()`
 *
 * Compile:
 *   gcc -O2 bare_runner.c -I../runners/native/wasm3/source ../runners/native/wasm3/source/*.c -lm -o bare_runner
 *
 * Usage:
 *   ./bare_runner <path-to-rom.wasm> [max_frames]
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "wasm3.h"
#include "m3_env.h"

static uint8_t *g_mem = NULL;
static size_t   g_mem_len = 0;
static uint32_t g_arena = 0x8000;

static void refresh_memory(IM3Runtime runtime) {
    if (runtime && runtime->modules) {
        g_mem = m3_GetMemory(runtime->modules, &g_mem_len, 0);
    }
}

static uint32_t host_alloc(IM3Runtime runtime, uint32_t size, uint32_t align) {
    refresh_memory(runtime);
    if (align > 1) g_arena = (g_arena + align - 1) & ~(align - 1);
    uint32_t ptr = g_arena;
    g_arena += size;
    return ptr;
}

static uint32_t g_logger_ptr = 0;
static uint32_t g_logger_buf_ptr = 0;

typedef struct {
    uint32_t buffer;
    uint32_t capacity;
    uint32_t length;
} bare_logger_t;

/* ── Host Capability Dispatcher: wextension(name) ── */
m3ApiRawFunction(host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);

    refresh_memory(runtime);
    if (!g_mem || name_ptr >= g_mem_len) {
        m3ApiReturn(0);
    }

    const char *name = (const char*)(g_mem + name_ptr);
    printf("[Host] ROM requested extension: \"%s\"\n", name);

    // Custom Extension Example: "logger"
    if (strcmp(name, "logger") == 0) {
        if (!g_logger_ptr) {
            g_logger_ptr = host_alloc(runtime, sizeof(bare_logger_t), 4);
            g_logger_buf_ptr = host_alloc(runtime, 1024, 4);

            bare_logger_t *log = (bare_logger_t*)(g_mem + g_logger_ptr);
            log->buffer = g_logger_buf_ptr;
            log->capacity = 1024;
            log->length = 0;
        }
        m3ApiReturn(g_logger_ptr);
    }

    // Unknown extension: return NULL
    m3ApiReturn(0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <path-to-rom.wasm> [max_frames]\n", argv[0]);
        return 1;
    }

    const char *rom_path = argv[1];
    int max_frames = (argc >= 3) ? atoi(argv[2]) : 60;
    if (max_frames <= 0) max_frames = 60;

    // Read WASM binary
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Could not open %s\n", rom_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *wasm_buf = (uint8_t*)malloc(sz);
    fread(wasm_buf, 1, sz, f);
    fclose(f);

    // Initialize Wasm3 environment
    IM3Environment env = m3_NewEnvironment();
    IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, NULL);

    IM3Module module;
    M3Result res = m3_ParseModule(env, &module, wasm_buf, sz);
    if (res) { fprintf(stderr, "ParseModule error: %s\n", res); return 1; }

    res = m3_LoadModule(runtime, module);
    if (res) { fprintf(stderr, "LoadModule error: %s\n", res); return 1; }

    // Link wextension import
    m3_LinkRawFunction(module, "env", "wextension", "i(i)", &host_wextension);

    // Find wupdate export
    IM3Function func_wupdate;
    res = m3_FindFunction(&func_wupdate, runtime, "wupdate");
    if (res) { fprintf(stderr, "Error: wupdate export not found: %s\n", res); return 1; }

    printf("[Host] Starting execution loop (%d frames)...\n", max_frames);

    int frame = 0;
    while (frame < max_frames) {
        res = m3_CallV(func_wupdate);
        if (res) {
            fprintf(stderr, "Runtime error at frame %d: %s\n", frame, res);
            return 1;
        }

        int32_t status = 0;
        m3_GetResultsV(func_wupdate, &status);

        // Check if guest wrote anything to logger extension
        if (g_logger_ptr) {
            refresh_memory(runtime);
            bare_logger_t *log = (bare_logger_t*)(g_mem + g_logger_ptr);
            if (log->length > 0) {
                printf("[Guest Log] %.*s\n", (int)log->length, (char*)(g_mem + g_logger_buf_ptr));
                log->length = 0; // Flush
            }
        }

        if (status == 1) { // WUPDATE_EXIT
            printf("[Host] ROM requested clean exit at frame %d.\n", frame);
            break;
        }
        if (status < 0) { // WUPDATE_ERROR
            fprintf(stderr, "[Host] ROM returned error code %d at frame %d.\n", status, frame);
            return 1;
        }

        frame++;
    }

    printf("[Host] Finished successfully after %d frame(s).\n", frame);

    m3_FreeRuntime(runtime);
    m3_FreeEnvironment(env);
    free(wasm_buf);
    return 0;
}
