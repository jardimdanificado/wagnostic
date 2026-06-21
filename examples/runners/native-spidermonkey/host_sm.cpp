/**
 * Wagnostic Full Host (SpiderMonkey C++ embedding)
 *
 * GPU-accelerated: GLSL shaders (GLSL 120), triple-buffered textures,
 * double PBO async upload, SDL2 audio, full input.
 *
 * g++ host_sm.cpp $(pkg-config --cflags --libs mozjs-140) -O2 -lSDL2 -lGL -o wagnostic-sm
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>

#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/CompilationAndEvaluation.h"
#include "js/GlobalObject.h"
#include "js/Initialization.h"
#include "js/SourceText.h"
#include "js/Utility.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];
    uint32_t width, height, bpp, scale;
    uint32_t audio_size, audio_write, audio_read, audio_rate, audio_bpp, audio_channels;
    uint32_t ticks, gamepad;
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
static uint32_t W=320, H=240, BPP=8, SCALE=1;
static uint8_t* wasm_memory = NULL;

static GLuint render_program = 0;
static GLuint vram_textures[3] = {0};
static GLuint pbos[2] = {0};
static int tex_idx = 0, pbo_idx = 0, first_frame = 1;
static GLuint vao = 0, vbo = 0;
static GLint bpp_uniform_loc = -1;

static JSContext* gCx = NULL;

void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    if (!wasm_memory) return;
    SystemConfig* sys = (SystemConfig*)wasm_memory;
    uint8_t* audio_buf = wasm_memory + 512 + (W * H * (BPP / 8));
    float* stream = (float*)stream_ptr;
    int ns = len_bytes / sizeof(float);
    uint32_t r = sys->audio_read, w = sys->audio_write, sz = sys->audio_size;
    if (sz == 0) return;
    for (int i = 0; i < ns; i++) {
        if (r == w) { stream[i] = 0.0f; continue; }
        float s = 0;
        if (sys->audio_bpp == 1) { s = (audio_buf[r] - 128) / 128.0f; r = (r + 1) % sz; }
        else if (sys->audio_bpp == 2) { s = (*(int16_t*)(audio_buf + r)) / 32768.0f; r = (r + 2) % sz; }
        else { s = (*(float*)(audio_buf + r)); r = (r + 4) % sz; }
        stream[i] = s;
    }
    sys->audio_read = r;
}

// ── GLSL shaders (GLSL 130 — OpenGL 3.0+, has bitwise ops & texelFetch) ──
static const char* vertex_shader_src =
    "#version 130\n"
    "in vec2 pos;\n"
    "in vec2 uv;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    TexCoord = uv;\n"
    "}\n";

static const char* fragment_shader_src =
    "#version 130\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D vram;\n"
    "uniform int bpp;\n"
    "void main() {\n"
    "    ivec2 pix = ivec2(floor(TexCoord * vec2(textureSize(vram, 0))));\n"
    "    if (bpp == 8) {\n"
    "        int raw = int(texelFetch(vram, pix, 0).r * 255.0);\n"
    "        float r = float((raw >> 5) & 7) / 7.0;\n"
    "        float g = float((raw >> 2) & 7) / 7.0;\n"
    "        float b = float(raw & 3) / 3.0;\n"
    "        FragColor = vec4(r, g, b, 1.0);\n"
    "    } else if (bpp == 16) {\n"
    "        vec4 t = texelFetch(vram, pix, 0);\n"
    "        int p = int(t.r * 255.0) | (int(t.g * 255.0) << 8);\n"
    "        float r = float((p >> 11) & 0x1F) / 31.0;\n"
    "        float g = float((p >> 5) & 0x3F) / 63.0;\n"
    "        float b = float(p & 0x1F) / 31.0;\n"
    "        FragColor = vec4(r, g, b, 1.0);\n"
    "    } else {\n"
    "        FragColor = texelFetch(vram, pix, 0);\n"
    "    }\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &source, NULL);
    glCompileShader(sh);
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(sh, 512, NULL, log);
               fprintf(stderr, "Shader error: %s\n", log); }
    return sh;
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
        GLenum ifmt = (BPP == 8) ? GL_R8 : (BPP == 16) ? GL_RG8 : GL_RGBA8;
        GLenum base = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, W, H, 0, base, GL_UNSIGNED_BYTE, NULL);
    }
    if (pbos[0]) glDeleteBuffers(2, pbos);
    glGenBuffers(2, pbos);
    size_t vb = (size_t)W * H * (BPP / 8);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, vb, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    tex_idx = 0; pbo_idx = 0; first_frame = 1;
}

static void render_quad(GLuint tex_id) {
    glUseProgram(render_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(render_program, "vram"), 0);
    glUniform1i(bpp_uniform_loc, (GLint)BPP);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
}

static void upload_and_render() {
    uint8_t* vram = wasm_memory + 512;
    size_t vb = (size_t)W * H * (BPP / 8);
    GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
    int cur = pbo_idx, prev = 1 - pbo_idx;
    int rtex = (tex_idx + 2) % 3, utex = tex_idx;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[cur]);
    void* ptr = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY_ARB);
    if (ptr) memcpy(ptr, vram, vb);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[prev]);
    glBindTexture(GL_TEXTURE_2D, vram_textures[utex]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, fmt, GL_UNSIGNED_BYTE, NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    render_quad(vram_textures[rtex]);
    SDL_GL_SwapWindow(window);

    tex_idx = (tex_idx + 1) % 3;
    pbo_idx = 1 - pbo_idx;
}

static void init_sdl_from_header() {
    if (!wasm_memory) return;
    SystemConfig* sys = (SystemConfig*)wasm_memory;
    W = sys->width ? sys->width : 320;
    H = sys->height ? sys->height : 240;
    BPP = sys->bpp ? sys->bpp : 8;
    SCALE = sys->scale ? sys->scale : 1;

    if (!window) {
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic SM",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            W*SCALE, H*SCALE, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) return;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
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

static bool eval_js(const char* source) {
    JS::CompileOptions opts(gCx);
    opts.setFileAndLine("host.js", 1);
    JS::SourceText<mozilla::Utf8Unit> src;
    if (!src.init(gCx, (const mozilla::Utf8Unit*)source, strlen(source), JS::SourceOwnership::Borrowed))
        return false;
    JS::RootedValue rv(gCx);
    return JS::Evaluate(gCx, opts, src, &rv);
}

static bool refresh_memory() {
    JS::RootedObject g(gCx, JS::CurrentGlobalOrNull(gCx));
    JS::RootedValue fn(gCx);
    if (!JS_GetProperty(gCx, g, "__getMem", &fn) || !fn.isObject()) return false;
    JS::RootedValue mb(gCx);
    if (!JS_CallFunctionValue(gCx, g, fn, JS::HandleValueArray::empty(), &mb)) return false;
    if (!mb.isObject()) return false;
    bool shared; size_t len;
    JS::GetArrayBufferLengthAndData(&mb.toObject(), &len, &shared, &wasm_memory);
    return wasm_memory != nullptr;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s <rom.wasm>\n", argv[0]); return 1; }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("ROM"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = (uint8_t*)malloc(sz);
    fread(wasm_data, 1, sz, f); fclose(f);

    if (!JS_Init()) { fprintf(stderr, "JS_Init\n"); return 1; }
    gCx = JS_NewContext(64L * 1024L * 1024L);
    if (!gCx) { fprintf(stderr, "JS_NewContext\n"); return 1; }
    if (!JS::InitSelfHostedCode(gCx)) { fprintf(stderr, "InitSelfHosted\n"); return 1; }

    const JSClass cls = { "Wagnostic", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps };
    JS::RealmOptions ro;
    JS::RootedObject global(gCx, JS_NewGlobalObject(gCx, &cls, nullptr, JS::FireOnNewGlobalHook, ro));
    if (!global) { fprintf(stderr, "NewGlobalObject\n"); return 1; }

    { JSAutoRealm ar(gCx, global);
        auto copy = mozilla::UniquePtr<uint8_t[], JS::FreePolicy>((uint8_t*)JS_malloc(gCx, sz));
        if (!copy) { fprintf(stderr, "OOM\n"); return 1; }
        memcpy(copy.get(), wasm_data, sz);
        JS::RootedObject ab(gCx, JS::NewArrayBufferWithContents(gCx, sz, std::move(copy)));
        JS::RootedValue av(gCx, JS::ObjectValue(*ab));
        JS_DefineProperty(gCx, global, "__wasmBytes", av, JSPROP_ENUMERATE);
    }
    free(wasm_data);

    { JSAutoRealm ar(gCx, global);
        if (!eval_js(
            "var b=new Uint8Array(__wasmBytes);var m=new WebAssembly.Module(b);"
            "var inst=new WebAssembly.Instance(m,{env:{}});var e=inst.exports;e.winit();"
            "__wupdate=function(){e.wupdate();};"
            "__getMem=function(){return e.memory.buffer;};"
        )) { fprintf(stderr, "Setup failed\n"); return 1; }
    }

    { JSAutoRealm ar(gCx, global);
        if (!refresh_memory()) { fprintf(stderr, "Memory\n"); return 1; }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    init_sdl_from_header();

    int running = 1;
    while (running) {
        if (!wasm_memory) { SDL_Delay(1); continue; }
        SystemConfig* sys = (SystemConfig*)wasm_memory;
        sys->ticks = SDL_GetTicks();

        SDL_Event ev;
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
            if (ev.type == SDL_MOUSEWHEEL) sys->mouse_wheel += ev.wheel.y;
        }

        { JSAutoRealm ar(gCx, global);
            eval_js("__wupdate();");
            refresh_memory();
        }
        if (!wasm_memory) { running = 0; break; }
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
                GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
                glBindTexture(GL_TEXTURE_2D, vram_textures[0]);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, fmt, GL_UNSIGNED_BYTE, vram);
                tex_idx = 1; first_frame = 0;
                glClear(GL_COLOR_BUFFER_BIT);
                render_quad(vram_textures[0]);
                SDL_GL_SwapWindow(window);
            } else {
                upload_and_render();
            }
        }
        SDL_Delay(1);
    }

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    glDeleteBuffers(2, pbos);
    glDeleteTextures(3, vram_textures);
    glDeleteProgram(render_program);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    if (gl_ctx) SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window); SDL_Quit();
    JS_DestroyContext(gCx); JS_ShutDown();
    return 0;
}
