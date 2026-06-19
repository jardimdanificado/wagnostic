#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include <libplatform/libplatform.h>
#include <v8.h>

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

static uint8_t* wasm_memory = NULL;
static v8::Isolate* isolate = NULL;
static v8::Global<v8::Context>* global_context = NULL;
static v8::Global<v8::Function>* global_wupdate = NULL;
static v8::Global<v8::Object>* global_exports = NULL;

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

    FILE* f = fopen(wasm_path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", wasm_path); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = (uint8_t*)malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);

    // Init V8
    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate = v8::Isolate::New(create_params);

    v8::Global<v8::Function> func_winit_g, func_wupdate_g;
    uint32_t W=320, H=240, BPP=8;

    {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        auto context = v8::Context::New(isolate);
        v8::Context::Scope context_scope(context);
        global_context = new v8::Global<v8::Context>(isolate, context);

        v8::MemorySpan<const uint8_t> wire_bytes(wasm_data, sz);
        auto module = v8::WasmModuleObject::Compile(isolate, wire_bytes).ToLocalChecked();

        auto global = context->Global();
        auto webassembly = global->Get(context,
            v8::String::NewFromUtf8Literal(isolate, "WebAssembly")).ToLocalChecked().As<v8::Object>();
        auto instantiate_fn = webassembly->Get(context,
            v8::String::NewFromUtf8Literal(isolate, "instantiate")).ToLocalChecked().As<v8::Function>();

        v8::Local<v8::Value> args[] = { module };
        auto result = instantiate_fn->Call(context, webassembly, 1, args).ToLocalChecked();
        auto promise = result.As<v8::Promise>();
        while (promise->State() == v8::Promise::kPending) {
            isolate->PerformMicrotaskCheckpoint();
        }

        auto instance = promise->Result().As<v8::Object>();
        auto exports = instance->Get(context,
            v8::String::NewFromUtf8Literal(isolate, "exports")).ToLocalChecked().As<v8::Object>();
        global_exports = new v8::Global<v8::Object>(isolate, exports);

        // Get functions
        auto winit_v = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "winit")).ToLocalChecked();
        auto wupdate_v = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "wupdate")).ToLocalChecked();
        if (winit_v->IsFunction())
            func_winit_g.Reset(isolate, winit_v.As<v8::Function>());
        if (wupdate_v->IsFunction())
            func_wupdate_g.Reset(isolate, wupdate_v.As<v8::Function>());
        global_wupdate = &func_wupdate_g;

        // Get memory
        auto mem_val = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "memory")).ToLocalChecked();
        if (mem_val->IsWasmMemoryObject()) {
            auto mem_obj = mem_val.As<v8::WasmMemoryObject>();
            auto buf = mem_obj->Buffer();
            wasm_memory = (uint8_t*)buf->GetBackingStore()->Data();
        }

        // Call winit
        if (!func_winit_g.IsEmpty()) {
            auto winit_fn = func_winit_g.Get(isolate);
            winit_fn->Call(context, context->Global(), 0, nullptr).ToLocalChecked();
            isolate->PerformMicrotaskCheckpoint();
        }

        // Re-fetch memory after init
        mem_val = exports->Get(context, v8::String::NewFromUtf8Literal(isolate, "memory")).ToLocalChecked();
        if (mem_val->IsWasmMemoryObject()) {
            auto mem_obj = mem_val.As<v8::WasmMemoryObject>();
            wasm_memory = (uint8_t*)mem_obj->Buffer()->GetBackingStore()->Data();
        }
    }

    SystemConfig* sys = (SystemConfig*)wasm_memory;
    W = sys->width ? sys->width : 320;
    H = sys->height ? sys->height : 240;
    BPP = sys->bpp ? sys->bpp : 8;
    uint64_t vram_bytes = (uint64_t)W * H * (BPP / 8);
    int has_audio = sys->audio_size > 0;

    printf("========================================\n");
    printf("  WAGNOSTIC BENCHMARK (V8)\n");
    printf("========================================\n");
    printf("ROM:          %s\n", wasm_path);
    printf("Title:        %s\n", sys->message);
    printf("Resolution:   %ux%u @ %ubpp\n", W, H, BPP);
    printf("VRAM:         %lu KB\n", (unsigned long)(vram_bytes / 1024));
    printf("Engine:       V8 (TurboFan JIT)\n");
    if (has_audio) {
        printf("Audio:        %u Hz, %u-bit, %u ch, %u KB buffer\n",
               sys->audio_rate, sys->audio_bpp*8, sys->audio_channels, sys->audio_size/1024);
    } else { printf("Audio:        none\n"); }
    printf("Frames:       %d\n", num_frames);
    printf("========================================\n");
    printf("Running benchmark...\n");
    fflush(stdout);

    // Re-enter isolate for frame loop
    uint64_t start_ns = now_ns();
    uint64_t total_update_ns = 0;
    uint64_t min_frame_ns = UINT64_MAX, max_frame_ns = 0;

    for (int i = 0; i < num_frames; i++) {
        v8::Isolate::Scope isolate_scope(isolate);
        v8::HandleScope handle_scope(isolate);
        auto context = global_context->Get(isolate);
        v8::Context::Scope context_scope(context);

        sys = (SystemConfig*)wasm_memory;
        sys->ticks = (uint32_t)(i * 16);

        uint64_t frame_start = now_ns();

        // Call wupdate
        if (!global_wupdate->IsEmpty()) {
            auto fn = global_wupdate->Get(isolate);
            fn->Call(context, context->Global(), 0, nullptr).ToLocalChecked();
            isolate->PerformMicrotaskCheckpoint();
        }

        // Re-fetch memory (V8 may have grown it)
        if (global_exports) {
            auto exports = global_exports->Get(isolate);
            auto mem_val = exports->Get(context,
                v8::String::NewFromUtf8Literal(isolate, "memory")).ToLocalChecked();
            if (mem_val->IsWasmMemoryObject()) {
                auto mem_obj = mem_val.As<v8::WasmMemoryObject>();
                wasm_memory = (uint8_t*)mem_obj->Buffer()->GetBackingStore()->Data();
            }
        }
        // Clear signals
        sys = (SystemConfig*)wasm_memory;
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
    printf("  RESULTS (V8)\n");
    printf("========================================\n");
    printf("Total time:       %.2f ms\n", total_ms);
    printf("Avg frame time:   %.3f ms\n", avg_ms);
    printf("Min frame time:   %.3f ms\n", (double)min_frame_ns/1e6);
    printf("Max frame time:   %.3f ms\n", (double)max_frame_ns/1e6);
    printf("Avg FPS:          %.1f\n", fps);
    printf("Pixels/sec:       %.2f MP/s\n", mpps);
    printf("VRAM bandwidth:   %.2f MB/s\n", bw);
    printf("Engine:           V8 (TurboFan JIT)\n");
    printf("========================================\n");

    // Skip V8 cleanup to avoid monolith boundary issues with free()
    // OS will reclaim all resources on exit
    free(wasm_data);
    fflush(stdout);
    _Exit(0);
}
