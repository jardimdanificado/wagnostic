#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <span>
#include <SDL2/SDL.h>

#if defined(_WIN32) && defined(_MSC_VER)
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif

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

static SDL_Window* window = NULL;
static SDL_GLContext gl_ctx = NULL;
static SDL_AudioDeviceID audio_dev = 0;
static GLuint render_program = 0;
static GLuint vao = 0, vbo = 0;
static uint32_t W=320, H=240, BPP=8, SCALE=1;
static GLint bpp_uniform_loc = -1;

static GLuint vram_textures[3];
static GLuint pbos[2];
static int tex_idx = 0;
static int pbo_idx = 0;
static int first_frame = 1;

static v8::Isolate* v8_isolate = NULL;
static v8::Local<v8::Context> v8_context;
static v8::Local<v8::Object> v8_exports;
static v8::Local<v8::Function> v8_func_winit;
static v8::Local<v8::Function> v8_func_wupdate;
static uint8_t* wasm_memory = NULL;

void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    if (!wasm_memory) return;
    SystemConfig* sys = (SystemConfig*)wasm_memory;
    uint8_t* audio_buf = wasm_memory + 512 + (W * H * (BPP / 8));
    float* stream = (float*)stream_ptr;
    int num_samples = len_bytes / sizeof(float);

    uint32_t r = sys->audio_read;
    uint32_t w = sys->audio_write;
    uint32_t size = sys->audio_size;
    if (size == 0) return;

    for (int i = 0; i < num_samples; i++) {
        if (r == w) { stream[i] = 0.0f; continue; }
        float sample = 0;
        if (sys->audio_bpp == 1) {
            sample = (audio_buf[r] - 128) / 128.0f;
            r = (r + 1) % size;
        } else if (sys->audio_bpp == 2) {
            sample = (*(int16_t*)(audio_buf + r)) / 32768.0f;
            r = (r + 2) % size;
        } else if (sys->audio_bpp == 4) {
            sample = (*(float*)(audio_buf + r));
            r = (r + 4) % size;
        }
        stream[i] = sample;
    }
    sys->audio_read = r;
}

static const char* vertex_shader_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    TexCoord = uv;\n"
    "}\n";

static const char* fragment_shader_src =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D vram;\n"
    "uniform int bpp;\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(floor(TexCoord * vec2(textureSize(vram, 0))));\n"
    "    vec4 color;\n"
    "    if (bpp == 8) {\n"
    "        uint raw = uint(texelFetch(vram, pix, 0).r * 255.0);\n"
    "        float r = float((raw >> 5) & 0x07u) / 7.0;\n"
    "        float g = float((raw >> 2) & 0x07u) / 7.0;\n"
    "        float b = float(raw & 0x03u) / 3.0;\n"
    "        color = vec4(r, g, b, 1.0);\n"
    "    } else if (bpp == 16) {\n"
    "        uvec2 raw = uvec2(texelFetch(vram, pix, 0).rg * 255.0);\n"
    "        uint packed = raw.x | (raw.y << 8);\n"
    "        float r = float((packed >> 11) & 0x1Fu) / 31.0;\n"
    "        float g = float((packed >> 5) & 0x3Fu) / 63.0;\n"
    "        float b = float(packed & 0x1Fu) / 31.0;\n"
    "        color = vec4(r, g, b, 1.0);\n"
    "    } else {\n"
    "        color = texelFetch(vram, pix, 0);\n"
    "    }\n"
    "    FragColor = color;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(shader, 512, NULL, log);
               fprintf(stderr, "Shader error: %s\n", log); }
    return shader;
}

static void init_gpu_pipeline() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    render_program = glCreateProgram();
    glAttachShader(render_program, vs);
    glAttachShader(render_program, fs);
    glLinkProgram(render_program);
    glDeleteShader(vs); glDeleteShader(fs);

    bpp_uniform_loc = glGetUniformLocation(render_program, "bpp");

    float quad[] = {
        -1,-1, 0,0,  1,-1, 1,0,  -1,1, 0,1,  1,1, 1,1
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*4, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(2*4));
    glBindVertexArray(0);
}

static void init_textures_and_pbos() {
    for (int i = 0; i < 3; i++) {
        if (vram_textures[i]) glDeleteTextures(1, &vram_textures[i]);
        glGenTextures(1, &vram_textures[i]);
        glBindTexture(GL_TEXTURE_2D, vram_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (BPP == 8)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        else if (BPP == 16)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, W, H, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    if (pbos[0]) glDeleteBuffers(2, pbos);
    glGenBuffers(2, pbos);
    size_t vram_bytes = (size_t)W * H * (BPP / 8);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, vram_bytes, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    tex_idx = 0;
    pbo_idx = 0;
    first_frame = 1;
}

static void init_sdl_from_header() {
    if (!wasm_memory) return;
    SystemConfig* sys = (SystemConfig*)wasm_memory;

    W = sys->width; H = sys->height; BPP = sys->bpp; SCALE = sys->scale;
    if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 8; if (SCALE == 0) SCALE = 1;

    if (!window) {
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic V8",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            W*SCALE, H*SCALE, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) return;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        gl_ctx = SDL_GL_CreateContext(window);
        SDL_GL_SetSwapInterval(1);
        init_gpu_pipeline();
    } else {
        SDL_SetWindowSize(window, W*SCALE, H*SCALE);
    }

    glViewport(0, 0, W*SCALE, H*SCALE);
    init_textures_and_pbos();

    if (sys->audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = sys->audio_rate ? sys->audio_rate : 44100;
        wanted.format = AUDIO_F32;
        wanted.channels = sys->audio_channels ? sys->audio_channels : 1;
        wanted.samples = 1024;
        wanted.callback = host_audio_callback;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }
}

static void upload_and_render() {
    uint8_t* vram = wasm_memory + 512;
    size_t vram_bytes = (size_t)W * H * (BPP / 8);

    GLenum fmt, ifmt;
    if (BPP == 8)       { fmt = GL_RED;  ifmt = GL_R8; }
    else if (BPP == 16) { fmt = GL_RG;   ifmt = GL_RG8; }
    else                { fmt = GL_RGBA; ifmt = GL_RGBA8; }

    int cur_pbo = pbo_idx;
    int prev_pbo = 1 - pbo_idx;
    int render_tex = (tex_idx + 2) % 3;
    int upload_tex = tex_idx;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[cur_pbo]);
    void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, vram_bytes,
                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (ptr) memcpy(ptr, vram, vram_bytes);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[prev_pbo]);
    glBindTexture(GL_TEXTURE_2D, vram_textures[upload_tex]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, fmt, GL_UNSIGNED_BYTE, NULL);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(render_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, vram_textures[render_tex]);
    glUniform1i(glGetUniformLocation(render_program, "vram"), 0);
    glUniform1i(bpp_uniform_loc, BPP);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(window);

    tex_idx = (tex_idx + 1) % 3;
    pbo_idx = 1 - pbo_idx;
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    FILE* f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = (uint8_t*)malloc(sz); fread(wasm_data, 1, sz, f); fclose(f);

    v8::V8::InitializeICUDefaultLocation(argv[0]);
    v8::V8::InitializeExternalStartupData(argv[0]);
    std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    v8_isolate = v8::Isolate::New(create_params);

    {
        v8::Isolate::Scope isolate_scope(v8_isolate);
        v8::HandleScope handle_scope(v8_isolate);
        v8_context = v8::Context::New(v8_isolate);
        v8::Context::Scope context_scope(v8_context);

        v8::Local<v8::ArrayBuffer> wasm_buffer = v8::ArrayBuffer::New(v8_isolate, sz);
        memcpy(wasm_buffer->GetBackingStore()->Data(), wasm_data, sz);

        std::span<const uint8_t> wire_bytes(static_cast<const uint8_t*>(wasm_data), sz);
        v8::Local<v8::WasmModuleObject> module = v8::WasmModuleObject::Compile(v8_isolate, wire_bytes).ToLocalChecked();

        v8::Local<v8::Value> global_webassembly = v8_context->Global()->Get(v8_context,
            v8::String::NewFromUtf8Literal(v8_isolate, "WebAssembly")).ToLocalChecked();
        v8::Local<v8::Object> webassembly = v8::Local<v8::Object>::Cast(global_webassembly);

        v8::Local<v8::Function> instantiate = v8::Local<v8::Function>::Cast(
            webassembly->Get(v8_context, v8::String::NewFromUtf8Literal(v8_isolate, "instantiate")).ToLocalChecked());

        v8::Local<v8::Value> args[] = { module };
        v8::Local<v8::Value> result = instantiate->Call(v8_context, webassembly, 1, args).ToLocalChecked();

        v8::Local<v8::Promise> promise = v8::Local<v8::Promise>::Cast(result);
        while (promise->State() == v8::Promise::kPending) {
            v8_isolate->PerformMicrotaskCheckpoint();
        }
        v8::Local<v8::Value> instance_val = promise->Result();
        v8::Local<v8::Object> instance = v8::Local<v8::Object>::Cast(instance_val);

        v8_exports = v8::Local<v8::Object>::Cast(
            instance->Get(v8_context, v8::String::NewFromUtf8Literal(v8_isolate, "exports")).ToLocalChecked());

        v8::Local<v8::Value> winit_val = v8_exports->Get(v8_context, 
            v8::String::NewFromUtf8Literal(v8_isolate, "winit")).ToLocalChecked();
        v8::Local<v8::Value> wupdate_val = v8_exports->Get(v8_context, 
            v8::String::NewFromUtf8Literal(v8_isolate, "wupdate")).ToLocalChecked();

        if (winit_val->IsFunction()) v8_func_winit = v8::Local<v8::Function>::Cast(winit_val);
        if (wupdate_val->IsFunction()) v8_func_wupdate = v8::Local<v8::Function>::Cast(wupdate_val);

        v8::Local<v8::Value> memory_val = v8_exports->Get(v8_context, 
            v8::String::NewFromUtf8Literal(v8_isolate, "memory")).ToLocalChecked();
        if (memory_val->IsWasmMemoryObject()) {
            v8::Local<v8::WasmMemoryObject> memory_obj = v8::Local<v8::WasmMemoryObject>::Cast(memory_val);
            v8::Local<v8::ArrayBuffer> memory_buf = memory_obj->Buffer();
            wasm_memory = static_cast<uint8_t*>(memory_buf->GetBackingStore()->Data());
        }

        if (!v8_func_winit.IsEmpty()) {
            v8_func_winit->Call(v8_context, v8_context->Global(), 0, nullptr).ToLocalChecked();
            v8_isolate->PerformMicrotaskCheckpoint();
            memory_val = v8_exports->Get(v8_context, 
                v8::String::NewFromUtf8Literal(v8_isolate, "memory")).ToLocalChecked();
            if (memory_val->IsWasmMemoryObject()) {
                v8::Local<v8::WasmMemoryObject> memory_obj = v8::Local<v8::WasmMemoryObject>::Cast(memory_val);
                v8::Local<v8::ArrayBuffer> memory_buf = memory_obj->Buffer();
                wasm_memory = static_cast<uint8_t*>(memory_buf->GetBackingStore()->Data());
            }
        }

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
        init_sdl_from_header();

        int running = 1;
        while (running) {
            SDL_Event ev;
            if (!wasm_memory) { SDL_Delay(1); continue; }
            SystemConfig* sys = (SystemConfig*)wasm_memory;
            sys->ticks = SDL_GetTicks();

            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = 0;
                if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                    if (ev.key.keysym.scancode < 256) sys->keys[ev.key.keysym.scancode] = (ev.type == SDL_KEYDOWN);
                }
                if (ev.type == SDL_MOUSEMOTION) { sys->mouse_x = ev.motion.x / SCALE; sys->mouse_y = ev.motion.y / SCALE; }
                if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                    int b = ev.button.button;
                    if (b == SDL_BUTTON_LEFT) sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 1) : (sys->mouse_buttons & ~1);
                    if (b == SDL_BUTTON_RIGHT) sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 2) : (sys->mouse_buttons & ~2);
                }
                if (ev.type == SDL_MOUSEWHEEL) {
                    sys->mouse_wheel += ev.wheel.y;
                }
            }

            if (!v8_func_wupdate.IsEmpty()) {
                v8_func_wupdate->Call(v8_context, v8_context->Global(), 0, nullptr).ToLocalChecked();
                v8_isolate->PerformMicrotaskCheckpoint();
                memory_val = v8_exports->Get(v8_context, 
                    v8::String::NewFromUtf8Literal(v8_isolate, "memory")).ToLocalChecked();
                if (memory_val->IsWasmMemoryObject()) {
                    v8::Local<v8::WasmMemoryObject> memory_obj = v8::Local<v8::WasmMemoryObject>::Cast(memory_val);
                    v8::Local<v8::ArrayBuffer> memory_buf = memory_obj->Buffer();
                    wasm_memory = static_cast<uint8_t*>(memory_buf->GetBackingStore()->Data());
                }
            }
            
            sys = (SystemConfig*)wasm_memory;

            int redraw = 0;
            for (int i = 0; i < 4; i++) {
                uint8_t s = sys->signals[i];
                if (s == 1) redraw = 1;
                else if (s == 2) running = 0;
                else if (s >= 3 && s <= 5) init_sdl_from_header();
                sys->signals[i] = 0;
            }
            sys->mouse_wheel = 0;

            if (redraw) {
                if (first_frame) {
                    uint8_t* vram = wasm_memory + 512;
                    size_t vram_bytes = (size_t)W * H * (BPP / 8);
                    GLenum fmt;
                    if (BPP == 8) fmt = GL_RED;
                    else if (BPP == 16) fmt = GL_RG;
                    else fmt = GL_RGBA;
                    glBindTexture(GL_TEXTURE_2D, vram_textures[0]);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, fmt, GL_UNSIGNED_BYTE, vram);
                    tex_idx = 1;
                    first_frame = 0;

                    glClear(GL_COLOR_BUFFER_BIT);
                    glUseProgram(render_program);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, vram_textures[0]);
                    glUniform1i(glGetUniformLocation(render_program, "vram"), 0);
                    glUniform1i(bpp_uniform_loc, BPP);
                    glBindVertexArray(vao);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    SDL_GL_SwapWindow(window);
                } else {
                    upload_and_render();
                }
            }
            SDL_Delay(1);
        }
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    glDeleteBuffers(2, pbos);
    glDeleteTextures(3, vram_textures);
    glDeleteProgram(render_program);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    v8_isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    free(wasm_data);
    return 0;
}
