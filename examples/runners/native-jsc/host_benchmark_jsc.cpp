/**
 * Wagnostic Benchmark Runner (JavaScriptCore C API)
 *
 * "V8-style" runner using WebKit's JavaScriptCore via its classic C API.
 * Compilation: g++ host_benchmark_jsc.cpp $(pkg-config --cflags --libs javascriptcoregtk-4.1) -O2 -o wagnostic-bench-jsc
 *
 * Uses JS to read SystemConfig fields since JSC's C API doesn't directly
 * expose WASM memory ArrayBuffer data. Benchmark loop calls wupdate via
 * JSObjectCallAsFunction() with nanosecond-precision timing.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <chrono>

#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JSTypedArray.h>

#ifndef kJSPropertyAttributeNone
#define kJSPropertyAttributeNone 0
#endif

static uint64_t now_ns(void) {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t).count();
}

static JSStringRef str(const char* s) {
    return JSStringCreateWithUTF8CString(s);
}

static void free_wasm(void* bytes, void* ctx) {
    (void)ctx; free(bytes);
}

// Evaluate JS and return the result, or null on error with exception printed
static JSValueRef eval_js(JSContextRef ctx, JSObjectRef global,
                          const char* source, const char* file) {
    JSStringRef src = str(source);
    JSStringRef f = str(file);
    JSValueRef exc = nullptr;
    JSValueRef r = JSEvaluateScript(ctx, src, global, f, 0, &exc);
    JSStringRelease(src);
    JSStringRelease(f);
    if (!r && exc) {
        JSStringRef es = JSValueToStringCopy(ctx, exc, nullptr);
        char buf[512];
        JSStringGetUTF8CString(es, buf, sizeof(buf));
        fprintf(stderr, "ERROR: %s\n", buf);
        JSStringRelease(es);
    }
    return r;
}

// Read a uint32 property from global
static uint32_t get_global_u32(JSContextRef ctx, JSObjectRef global,
                                const char* name) {
    JSStringRef p = str(name);
    JSValueRef exc = nullptr;
    JSValueRef v = JSObjectGetProperty(ctx, global, p, &exc);
    JSStringRelease(p);
    if (!v) return 0;
    return (uint32_t)JSValueToNumber(ctx, v, &exc);
}

// Read a string property from global (max 128 chars)
static void get_global_str(JSContextRef ctx, JSObjectRef global,
                            const char* name, char* out, size_t max) {
    JSStringRef p = str(name);
    JSValueRef exc = nullptr;
    JSValueRef v = JSObjectGetProperty(ctx, global, p, &exc);
    JSStringRelease(p);
    if (!v || !JSValueIsString(ctx, v)) { out[0] = 0; return; }
    JSStringRef s = JSValueToStringCopy(ctx, v, &exc);
    JSStringGetUTF8CString(s, out, (int)max);
    JSStringRelease(s);
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
    FILE* f = fopen(wasm_path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", wasm_path); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = (uint8_t*)malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);

    // ── JavaScriptCore initialisation ──
    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) { fprintf(stderr, "ERROR: JSGlobalContextCreate failed\n"); free(wasm_data); return 1; }
    JSObjectRef global = JSContextGetGlobalObject(ctx);

    // ── Inject WASM bytes via JS — create Uint8Array from passed data ──
    // We use a simple approach: read file in C, create ArrayBuffer via
    // JSObjectMakeArrayBufferWithBytesNoCopy, set as global __wasmBytes.
    void* copy = malloc(sz);
    memcpy(copy, wasm_data, sz);
    free(wasm_data);

    JSValueRef exc = nullptr;
    JSObjectRef ab = JSObjectMakeArrayBufferWithBytesNoCopy(
        ctx, copy, sz, free_wasm, nullptr, &exc);
    if (!ab) { fprintf(stderr, "ERROR: MakeArrayBuffer failed\n"); return 1; }
    JSStringRef abProp = str("__wasmBytes");
    JSObjectSetProperty(ctx, global, abProp, ab, kJSPropertyAttributeNone, &exc);
    JSStringRelease(abProp);

    // ── Evaluate setup JS ──
    // This creates the module, instance, calls winit, and stores all needed
    // values in global variables for C to read later.
    const char* setup =
        "var bytes = new Uint8Array(__wasmBytes);"
        "var module = new WebAssembly.Module(bytes);"
        "var instance = new WebAssembly.Instance(module, {env:{}});"
        "var e = instance.exports;"
        "e.winit();"
        // Store helpers on global for C to use
        "__wupdate = e.wupdate;"
        // Read SystemConfig fields from memory via DataView
        "var dv = new DataView(e.memory.buffer);"
        "__sys_title = '';"
        "for (var __i = 0; __i < 128; __i++) { var __c = dv.getUint8(__i); if (!__c) break; __sys_title += String.fromCharCode(__c); }"
        "__sys_width  = dv.getUint32(128, true);"
        "__sys_height = dv.getUint32(132, true);"
        "__sys_bpp    = dv.getUint32(136, true);"
        "__sys_audioSize = dv.getUint32(144, true);"
        "__sys_audioRate = dv.getUint32(156, true);"
        "__sys_audioBpp  = dv.getUint32(160, true);"
        "__sys_audioCh   = dv.getUint32(164, true);";

    JSValueRef setupResult = eval_js(ctx, global, setup, "setup.js");
    if (!setupResult) { JSGlobalContextRelease(ctx); return 1; }

    // ── Read SystemConfig from JS globals ──
    uint32_t W  = get_global_u32(ctx, global, "__sys_width")  ?: 320;
    uint32_t H  = get_global_u32(ctx, global, "__sys_height") ?: 240;
    uint32_t BPP = get_global_u32(ctx, global, "__sys_bpp")   ?: 8;
    uint64_t vram_bytes = (uint64_t)W * H * (BPP / 8);

    char title[128] = "";
    get_global_str(ctx, global, "__sys_title", title, sizeof(title));

    uint32_t audio_size = get_global_u32(ctx, global, "__sys_audioSize");
    uint32_t audio_rate = get_global_u32(ctx, global, "__sys_audioRate");
    uint32_t audio_bpp  = get_global_u32(ctx, global, "__sys_audioBpp");
    uint32_t audio_ch   = get_global_u32(ctx, global, "__sys_audioCh");
    int has_audio = audio_size > 0;

    printf("========================================\n");
    printf("  WAGNOSTIC BENCHMARK (JavaScriptCore)\n");
    printf("========================================\n");
    printf("ROM:          %s\n", wasm_path);
    printf("Title:        %s\n", title);
    printf("Resolution:   %ux%u @ %ubpp\n", W, H, BPP);
    printf("VRAM:         %lu KB\n", (unsigned long)(vram_bytes / 1024));
    printf("Engine:       JavaScriptCore (WebKit)\n");
    if (has_audio) {
        printf("Audio:        %u Hz, %u-bit, %u ch, %u KB buffer\n",
               audio_rate, audio_bpp*8, audio_ch, audio_size/1024);
    } else { printf("Audio:        none\n"); }
    printf("Frames:       %d\n", num_frames);
    printf("========================================\n");
    printf("Running benchmark...\n");
    fflush(stdout);

    // ── Get wupdate function reference ──
    JSStringRef wupdProp = str("__wupdate");
    JSValueRef wupdVal = JSObjectGetProperty(ctx, global, wupdProp, &exc);
    JSStringRelease(wupdProp);
    if (!wupdVal || !JSValueIsObject(ctx, wupdVal)) {
        fprintf(stderr, "ERROR: __wupdate not found\n");
        JSGlobalContextRelease(ctx);
        return 1;
    }
    JSObjectRef wupdFn = (JSObjectRef)wupdVal;

    // ── Benchmark loop ──
    uint64_t start_ns = now_ns();
    uint64_t total_update_ns = 0;
    uint64_t min_frame_ns = UINT64_MAX, max_frame_ns = 0;

    for (int i = 0; i < num_frames; i++) {
        uint64_t frame_start = now_ns();

        // Skip tick-setting and signal-clearing — they don't affect
        // raw WASM execution time which is what we're measuring.
        exc = nullptr;
        JSValueRef callResult = JSObjectCallAsFunction(ctx, wupdFn, global, 0, nullptr, &exc);
        if (!callResult) {
            fprintf(stderr, "\nERROR: wupdate failed at frame %d\n", i);
            break;
        }

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
    printf("  RESULTS (JavaScriptCore)\n");
    printf("========================================\n");
    printf("Total time:       %.2f ms\n", total_ms);
    printf("Avg frame time:   %.3f ms\n", avg_ms);
    printf("Min frame time:   %.3f ms\n", (double)min_frame_ns/1e6);
    printf("Max frame time:   %.3f ms\n", (double)max_frame_ns/1e6);
    printf("Avg FPS:          %.1f\n", fps);
    printf("Pixels/sec:       %.2f MP/s\n", mpps);
    printf("VRAM bandwidth:   %.2f MB/s\n", bw);
    printf("Engine:           JavaScriptCore (WebKit)\n");
    printf("========================================\n");

    JSGlobalContextRelease(ctx);
    return 0;
}
