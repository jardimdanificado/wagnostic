/**
 * Wagnostic Benchmark Runner (SpiderMonkey C++ API)
 *
 * "V8-style" runner using libmozjs-140 directly via C++ embedding API.
 * Compilation: g++ host_benchmark_sm.cpp $(pkg-config --cflags --libs mozjs-140) -O2 -o wagnostic-bench-sm
 *
 * Uses JS evaluation for WASM instantiation, then C++ API for the benchmark
 * loop with nanosecond-precision timing (consistent with other runners).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <chrono>

#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/CompilationAndEvaluation.h"
#include "js/GlobalObject.h"
#include "js/Initialization.h"
#include "js/SourceText.h"
#include "js/CharacterEncoding.h"
#include "js/Utility.h"  // JS::FreePolicy

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

static const JSClass global_class = {
    "WagnosticGlobal",
    JSCLASS_GLOBAL_FLAGS,
    &JS::DefaultGlobalClassOps
};

static uint64_t now_ns(void) {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
}

// Read entire file into heap buffer
static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", path); return nullptr; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

// Evaluate a JS string and return the result
static bool eval_js(JSContext* cx, const char* utf8, size_t len,
                    JS::MutableHandleValue rval) {
    JS::CompileOptions opts(cx);
    opts.setFileAndLine("benchmark", 1);
    JS::SourceText<mozilla::Utf8Unit> src;
    if (!src.init(cx, (const mozilla::Utf8Unit*)utf8, len,
                  JS::SourceOwnership::Borrowed)) {
        return false;
    }
    return JS::Evaluate(cx, opts, src, rval);
}

// Get a property from global and assign to a rooted value
static bool get_global(JSContext* cx, const char* name,
                       JS::MutableHandleValue val) {
    JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
    if (!global) return false;
    return JS_GetProperty(cx, global, name, val);
}

// Get an object property
static bool get_prop(JSContext* cx, JS::HandleObject obj, const char* name,
                     JS::MutableHandleValue val) {
    return JS_GetProperty(cx, obj, name, val);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rom.wasm> <num_frames>\n", argv[0]);
        return 1;
    }

    const char* wasm_path = argv[1];
    int num_frames = atoi(argv[2]);
    if (num_frames <= 0) num_frames = 100;

    // Read WASM file
    size_t wasm_size = 0;
    uint8_t* wasm_bytes = read_file(wasm_path, &wasm_size);
    if (!wasm_bytes) return 1;

    // ── SpiderMonkey initialisation ──
    if (!JS_Init()) {
        fprintf(stderr, "ERROR: JS_Init failed\n");
        free(wasm_bytes);
        return 1;
    }

    JSContext* cx = JS_NewContext(64L * 1024L * 1024L); // 64 MB stack
    if (!cx) {
        fprintf(stderr, "ERROR: JS_NewContext failed\n");
        JS_ShutDown();
        free(wasm_bytes);
        return 1;
    }

    if (!JS::InitSelfHostedCode(cx)) {
        fprintf(stderr, "ERROR: InitSelfHostedCode failed\n");
        JS_DestroyContext(cx);
        JS_ShutDown();
        free(wasm_bytes);
        return 1;
    }

    JS::RealmOptions realm_opts;
    JS::RootedObject global(cx,
        JS_NewGlobalObject(cx, &global_class, nullptr,
                           JS::FireOnNewGlobalHook, realm_opts));
    if (!global) {
        fprintf(stderr, "ERROR: JS_NewGlobalObject failed\n");
        JS_DestroyContext(cx);
        JS_ShutDown();
        free(wasm_bytes);
        return 1;
    }

    JSAutoRealm ar(cx, global);

    // ── Inject WASM bytes into JS ──
    {
        auto copy = mozilla::UniquePtr<uint8_t[], JS::FreePolicy>(
            (uint8_t*)JS_malloc(cx, wasm_size));
        if (!copy) { fprintf(stderr, "ERROR: OOM\n"); return 1; }
        memcpy(copy.get(), wasm_bytes, wasm_size);
        JS::RootedObject ab(cx,
            JS::NewArrayBufferWithContents(cx, wasm_size, std::move(copy)));
        if (!ab) { fprintf(stderr, "ERROR: NewArrayBufferWithContents\n"); return 1; }
        JS::RootedValue ab_val(cx, JS::ObjectValue(*ab));
        JS_DefineProperty(cx, global, "__wasmBytes", ab_val, JSPROP_ENUMERATE);
    }
    // wasm_bytes is NOT freed here — SpiderMonkey now owns the data via copy

    // ── Evaluate setup JS ──
    const char setup_js[] = R"(
        var bytes = new Uint8Array(__wasmBytes);
        var module = new WebAssembly.Module(bytes);
        var instance = new WebAssembly.Instance(module, {env:{}});
        var exports = instance.exports;
        __winit = exports.winit;
        __wupdate = exports.wupdate;
        __memory = exports.memory;
        // Call winit
        __winit();
        // Get initial memory buffer
        __getMem = function() { return __memory.buffer; };
    )";

    JS::RootedValue rval(cx);
    if (!eval_js(cx, setup_js, strlen(setup_js), &rval)) {
        fprintf(stderr, "ERROR: Setup JS evaluation failed\n");
        JS_DestroyContext(cx);
        JS_ShutDown();
        return 1;
    }

    // ── Read SystemConfig from initial memory ──
    JS::RootedValue getMemFn(cx);
    if (!get_global(cx, "__getMem", &getMemFn) || !getMemFn.isObject()) {
        fprintf(stderr, "ERROR: __getMem not found\n");
        return 1;
    }

    auto get_mem_ptr = [&]() -> uint8_t* {
        JS::RootedValue memBuf(cx);
        if (!JS_CallFunctionValue(cx, global, getMemFn,
                                  JS::HandleValueArray::empty(), &memBuf))
            return nullptr;
        if (!memBuf.isObject()) return nullptr;
        JSObject* bufObj = &memBuf.toObject();
        bool isShared;
        uint8_t* data;
        size_t len;
        JS::GetArrayBufferLengthAndData(bufObj, &len, &isShared, &data);
        return data;
    };

    uint8_t* mem = get_mem_ptr();
    if (!mem) {
        fprintf(stderr, "ERROR: Cannot access WASM memory\n");
        JS_DestroyContext(cx);
        JS_ShutDown();
        return 1;
    }

    SystemConfig* sys = (SystemConfig*)mem;
    uint32_t W = sys->width ? sys->width : 320;
    uint32_t H = sys->height ? sys->height : 240;
    uint32_t BPP = sys->bpp ? sys->bpp : 8;
    uint64_t vram_bytes = (uint64_t)W * H * (BPP / 8);
    int has_audio = sys->audio_size > 0;

    printf("========================================\n");
    printf("  WAGNOSTIC BENCHMARK (SpiderMonkey)\n");
    printf("========================================\n");
    printf("ROM:          %s\n", wasm_path);
    printf("Title:        %s\n", sys->message);
    printf("Resolution:   %ux%u @ %ubpp\n", W, H, BPP);
    printf("VRAM:         %lu KB\n", (unsigned long)(vram_bytes / 1024));
    printf("Engine:       SpiderMonkey (libmozjs-140)\n");
    if (has_audio) {
        printf("Audio:        %u Hz, %u-bit, %u ch, %u KB buffer\n",
               sys->audio_rate, sys->audio_bpp*8, sys->audio_channels,
               sys->audio_size/1024);
    } else { printf("Audio:        none\n"); }
    printf("Frames:       %d\n", num_frames);
    printf("========================================\n");
    printf("Running benchmark...\n");
    fflush(stdout);

    // ── Benchmark loop ──
    uint64_t start_ns = now_ns();
    uint64_t total_update_ns = 0;
    uint64_t min_frame_ns = UINT64_MAX, max_frame_ns = 0;

    for (int i = 0; i < num_frames; i++) {
        // Update ticks (simulate 16ms per frame = ~60fps)
        sys->ticks = (uint32_t)(i * 16);

        uint64_t frame_start = now_ns();

        // Call wupdate via JS
        JS::RootedValue wupd(cx);
        if (!get_global(cx, "__wupdate", &wupd) || !wupd.isObject()) {
            fprintf(stderr, "\nERROR: __wupdate not found at frame %d\n", i);
            return 1;
        }
        if (!JS_CallFunctionValue(cx, global, wupd,
                                  JS::HandleValueArray::empty(), &rval)) {
            fprintf(stderr, "\nERROR: wupdate failed at frame %d\n", i);
            return 1;
        }

        // Re-fetch memory (may have grown)
        mem = get_mem_ptr();
        if (!mem) {
            fprintf(stderr, "\nERROR: memory lost at frame %d\n", i);
            return 1;
        }
        sys = (SystemConfig*)mem;

        // Clear signals (host behavior)
        for (int s = 0; s < 4; s++) sys->signals[s] = 0;
        sys->mouse_wheel = 0;

        uint64_t frame_end = now_ns();
        uint64_t frame_ns = frame_end - frame_start;
        total_update_ns += frame_ns;
        if (frame_ns < min_frame_ns) min_frame_ns = frame_ns;
        if (frame_ns > max_frame_ns) max_frame_ns = frame_ns;

        if ((i + 1) % 10 == 0 || i == num_frames - 1) {
            printf("\r  Frame %d/%d (%.1f ms/frame avg)",
                   i+1, num_frames, (double)total_update_ns/(i+1)/1e6);
            fflush(stdout);
        }
    }

    uint64_t end_ns = now_ns();
    uint64_t total_ns = end_ns - start_ns;

    double total_ms = (double)total_ns / 1e6;
    double avg_ms = (double)total_update_ns / num_frames / 1e6;
    double fps = 1000.0 / avg_ms;
    double mpps = (double)W * H * num_frames / (total_ms / 1000.0) / 1e6;
    double bw = (double)vram_bytes * num_frames / total_ms * 1000.0 / (1024.0*1024.0);

    printf("\n\n");
    printf("========================================\n");
    printf("  RESULTS (SpiderMonkey)\n");
    printf("========================================\n");
    printf("Total time:       %.2f ms\n", total_ms);
    printf("Avg frame time:   %.3f ms\n", avg_ms);
    printf("Min frame time:   %.3f ms\n", (double)min_frame_ns/1e6);
    printf("Max frame time:   %.3f ms\n", (double)max_frame_ns/1e6);
    printf("Avg FPS:          %.1f\n", fps);
    printf("Pixels/sec:       %.2f MP/s\n", mpps);
    printf("VRAM bandwidth:   %.2f MB/s\n", bw);
    printf("Engine:           SpiderMonkey (libmozjs-140)\n");
    printf("========================================\n");

    // Cleanup
    JS_DestroyContext(cx);
    JS_ShutDown();
    // wasm data was handed over to SpiderMonkey; no free

    return 0;
}
