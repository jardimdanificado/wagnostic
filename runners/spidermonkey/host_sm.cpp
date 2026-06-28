/**
 * Wagnostic SpiderMonkey Host (Named Globals)
 *
 * GPU-accelerated: GLSL 130 shaders, triple-buffered textures,
 * double PBO async upload, SDL2 audio, full input.
 *
 * Uses WASM exported globals to read/write ROM state by name.
 * Each global contains a pointer (i32) into WASM linear memory;
 * the host dereferences that pointer to read/write the actual value.
 *
 * Build:
 *   g++ -std=c++17 $(pkg-config --cflags --libs mozjs-140) -O2 -lSDL2 -lGL \
 *       host_sm.cpp -o wagnostic-sm
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mutex.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>

#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/CompilationAndEvaluation.h"
#include "js/GlobalObject.h"
#include "js/Initialization.h"
#include "js/SourceText.h"
#include "js/Utility.h"

// ============================================================
// Rect struct (matches WASM Rect: { int x, y, w, h; } = 16 bytes)
// ============================================================

typedef struct { int x, y, w, h; } Rect;

// ============================================================
// Global state
// ============================================================

static SDL_Window*          window     = NULL;
static SDL_GLContext        gl_ctx     = NULL;
static SDL_AudioDeviceID    audio_dev  = 0;
static uint8_t*             wasm_memory = NULL;

// Video config (read from WASM globals after wupdate())
static uint32_t W = 320, H = 240, BPP = 16, SCALE = 1;

// Previous-frame config for change detection
static uint32_t prev_W = 0, prev_H = 0, prev_BPP = 0, prev_SCALE = 0;

// Previous audio config for change detection
static uint32_t prev_audio_rate = 0, prev_audio_channels = 0, prev_audio_size = 0;

// GPU pipeline state
static GLuint render_program = 0;
static GLuint vram_textures[3] = {0};
static GLuint pbos[2] = {0};
static int tex_idx = 0, pbo_idx = 0, first_frame = 1;
static GLuint vao = 0, vbo = 0;
static GLint bpp_uniform_loc = -1;

// SpiderMonkey
static JSContext* gCx = NULL;
static JS::Heap<JSObject*> gGlobal;

// ============================================================
// Global pointer cache
// ============================================================
// Each named WASM global holds an i32 pointer into linear memory.
// We cache these pointers so we can read/write the actual values
// directly from C++ without crossing into JS on every access.

static uint32_t gptr_width             = 0;
static uint32_t gptr_height            = 0;
static uint32_t gptr_bpp               = 0;
static uint32_t gptr_scale             = 0;
static uint32_t gptr_title             = 0;
static uint32_t gptr_vram              = 0;
static uint32_t gptr_mouse_x           = 0;
static uint32_t gptr_mouse_y           = 0;
static uint32_t gptr_mouse_buttons     = 0;
static uint32_t gptr_mouse_wheel       = 0;
static uint32_t gptr_keys              = 0;
static uint32_t gptr_gamepad_buttons   = 0;
static uint32_t gptr_ticks             = 0;
static uint32_t gptr_target_fps        = 0;
static uint32_t gptr_audio_size        = 0;
static uint32_t gptr_audio_sample_rate = 0;
static uint32_t gptr_audio_bpp         = 0;
static uint32_t gptr_audio_channels    = 0;
static uint32_t gptr_audio_write       = 0;
static uint32_t gptr_audio_read        = 0;
static uint32_t gptr_audio_buffer      = 0;
static uint32_t gptr_dirty_count       = 0;
static uint32_t gptr_dirty_rects       = 0;

/* Audio buffer lock. wupdate() (in the main thread, via SpiderMonkey)
 * writes the audio buffer, and host_audio_callback() (in SDL's audio
 * thread) reads it. On multi-core systems those two threads can run
 * truly concurrently, and a torn read by the audio thread shows up as
 * a sample-amplitude pop. We serialize with this mutex — the main
 * thread holds it around the wupdate() JS call, the audio thread holds
 * it for the whole callback. Writes are short (a few KB memcpy), so
 * the audio thread only ever blocks briefly. */
static SDL_mutex *g_audio_mutex = NULL;

// ============================================================
// Direct memory read / write helpers
// ============================================================

static inline uint32_t mem_u32(uint32_t ptr) {
    if (!wasm_memory || ptr == 0) return 0;
    return *(const uint32_t*)(wasm_memory + ptr);
}

static inline void mem_u32(uint32_t ptr, uint32_t v) {
    if (wasm_memory && ptr) *(uint32_t*)(wasm_memory + ptr) = v;
}

static inline int32_t mem_i32(uint32_t ptr) {
    return (int32_t)mem_u32(ptr);
}

static inline void mem_i32(uint32_t ptr, int32_t v) {
    mem_u32(ptr, (uint32_t)v);
}

static inline uint8_t mem_u8(uint32_t ptr) {
    if (!wasm_memory || ptr == 0) return 0;
    return wasm_memory[ptr];
}

static inline void mem_u8(uint32_t ptr, uint8_t v) {
    if (wasm_memory && ptr) wasm_memory[ptr] = v;
}

static inline void mem_str(uint32_t ptr, char* dst, int max) {
    if (!wasm_memory || ptr == 0) { dst[0] = '\0'; return; }
    strncpy(dst, (const char*)(wasm_memory + ptr), max - 1);
    dst[max - 1] = '\0';
}

static inline Rect mem_rect(uint32_t base, int index) {
    uint32_t off = base + (uint32_t)index * 16; // 4 x int32 = 16 bytes
    Rect r;
    r.x = mem_i32(off);
    r.y = mem_i32(off + 4);
    r.w = mem_i32(off + 8);
    r.h = mem_i32(off + 12);
    return r;
}

// ============================================================
// GLSL 130 shaders
// ============================================================

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

// ============================================================
// GPU pipeline
// ============================================================

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &source, NULL);
    glCompileShader(sh);
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, 512, NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return sh;
}

static void init_gpu_pipeline() {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    render_program = glCreateProgram();
    glAttachShader(render_program, vs);
    glAttachShader(render_program, fs);
    glLinkProgram(render_program);

    GLint linked;
    glGetProgramiv(render_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(render_program, 512, NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    bpp_uniform_loc = glGetUniformLocation(render_program, "bpp");

    // Fullscreen quad: position (x,y) + texcoord (u,v)
    // UV origin at bottom-left so texture reads top-to-bottom as expected
    float quad[] = {
        -1, -1,  0, 1,   // bottom-left
         1, -1,  1, 1,   // bottom-right
        -1,  1,  0, 0,   // top-left
         1,  1,  1, 0,   // top-right
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static void init_textures_and_pbos() {
    // Create / recreate triple-buffered textures
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

    // Create / recreate double-buffered PBOs for async upload
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
    uint8_t* vram = wasm_memory + gptr_vram;
    size_t vram_bytes = (size_t)W * H * (BPP / 8);
    GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
    int cur = pbo_idx, prev = 1 - pbo_idx;
    int rtex = (tex_idx + 2) % 3, utex = tex_idx;

    // Upload: map current PBO, copy VRAM, unmap
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[cur]);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) memcpy(ptr, vram, vram_bytes);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    // Transfer: previous PBO -> current texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[prev]);
    glBindTexture(GL_TEXTURE_2D, vram_textures[utex]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, fmt, GL_UNSIGNED_BYTE, NULL);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // Render: render the ready texture
    glClear(GL_COLOR_BUFFER_BIT);
    render_quad(vram_textures[rtex]);
    SDL_GL_SwapWindow(window);

    tex_idx = (tex_idx + 1) % 3;
    pbo_idx = 1 - pbo_idx;
}

// Upload a sub-region of VRAM to the current texture (partial dirty rect).
// Uses GL_UNPACK_ROW_LENGTH to skip untouched rows.
static void upload_dirty_rect(const Rect& r) {
    int x = r.x, y = r.y, w = r.w, h = r.h;
    // Clamp to texture bounds
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)W) w = (int)W - x;
    if (y + h > (int)H) h = (int)H - y;
    if (w <= 0 || h <= 0) return;

    uint8_t* vram = wasm_memory + gptr_vram;
    GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
    int bpp_bytes = BPP / 8;

    // VRAM is tightly packed: (y * W + x) * bpp_bytes offset, stride = W * bpp_bytes
    size_t src_offset = ((size_t)y * W + x) * bpp_bytes;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)W);
    glBindTexture(GL_TEXTURE_2D, vram_textures[tex_idx]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, fmt, GL_UNSIGNED_BYTE,
                    vram + src_offset);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

static void set_viewport_with_letterbox() {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    SDL_Rect dst;
    if (aspect_win > aspect_rom) {
        dst.h = win_h;
        dst.w = (int)(win_h * aspect_rom);
        dst.x = (win_w - dst.w) / 2;
        dst.y = 0;
    } else {
        dst.w = win_w;
        dst.h = (int)(win_w / aspect_rom);
        dst.x = 0;
        dst.y = (win_h - dst.h) / 2;
    }
    glViewport(dst.x, dst.y, dst.w, dst.h);
}

// ============================================================
// Mouse coordinate conversion (accounting for letterbox)
// ============================================================

static void convert_mouse_coords(int window_x, int window_y, int* rom_x, int* rom_y) {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    uint32_t rw = mem_u32(gptr_width);
    uint32_t rh = mem_u32(gptr_height);
    if (rw == 0) rw = 320;
    if (rh == 0) rh = 240;

    float aspect_rom = (float)rw / (float)rh;
    float aspect_win = (float)win_w / (float)win_h;

    SDL_Rect dst;
    if (aspect_win > aspect_rom) {
        dst.h = win_h;
        dst.w = (int)(win_h * aspect_rom);
        dst.x = (win_w - dst.w) / 2;
        dst.y = 0;
    } else {
        dst.w = win_w;
        dst.h = (int)(win_w / aspect_rom);
        dst.x = 0;
        dst.y = (win_h - dst.h) / 2;
    }

    float scale_x = (float)rw / dst.w;
    float scale_y = (float)rh / dst.h;

    *rom_x = (int)((window_x - dst.x) * scale_x);
    *rom_y = (int)((window_y - dst.y) * scale_y);

    if (*rom_x < 0) *rom_x = 0;
    if (*rom_x >= (int)rw) *rom_x = rw - 1;
    if (*rom_y < 0) *rom_y = 0;
    if (*rom_y >= (int)rh) *rom_y = rh - 1;
}

// ============================================================
// Audio callback (runs on SDL audio thread)
// ============================================================

static void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    if (!wasm_memory) return;

    /* Block the main thread from writing the audio buffer while we
     * read it. See g_audio_mutex declaration. */
    if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);

    uint32_t r   = mem_u32(gptr_audio_read);
    uint32_t w   = mem_u32(gptr_audio_write);
    uint32_t sz  = mem_u32(gptr_audio_size);
    uint32_t bpp = mem_u32(gptr_audio_bpp);
    uint32_t channels = mem_u32(gptr_audio_channels);
    if (sz == 0) return;

    uint8_t* audio_buf = wasm_memory + gptr_audio_buffer;
    float* stream = (float*)stream_ptr;
    int total_samples = len_bytes / (int)sizeof(float);

    // Decode samples from ring buffer
    for (int i = 0; i < total_samples; i++) {
        if (r == w) {
            stream[i] = 0.0f;
            continue;
        }
        float s = 0;
        if (bpp == 1) {
            s = (audio_buf[r] - 128) / 128.0f;
            r = (r + 1) % sz;
        } else if (bpp == 2) {
            s = (*(int16_t*)(audio_buf + r)) / 32768.0f;
            r = (r + 2) % sz;
        } else {
            s = *(float*)(audio_buf + r);
            r = (r + 4) % sz;
        }
        stream[i] = s;
    }
    mem_u32(gptr_audio_read, r);

    /* Release the audio buffer lock so the main thread can write again. */
    if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
}

// ============================================================
// SpiderMonkey helpers
// ============================================================

static bool eval_js(const char* source) {
    JS::CompileOptions opts(gCx);
    opts.setFileAndLine("host.js", 1);
    JS::SourceText<mozilla::Utf8Unit> src;
    if (!src.init(gCx, (const mozilla::Utf8Unit*)source, strlen(source),
                  JS::SourceOwnership::Borrowed))
        return false;
    JS::RootedValue rv(gCx);
    return JS::Evaluate(gCx, opts, src, &rv);
}

// Refresh wasm_memory after potential WASM memory growth.
// Must call __getMem() which returns memory.buffer (ArrayBuffer).
static bool refresh_memory() {
    JS::RootedObject g(gCx, gGlobal);
    JS::RootedValue fn(gCx);
    if (!JS_GetProperty(gCx, g, "__getMem", &fn) || !fn.isObject()) return false;
    JS::RootedValue mb(gCx);
    if (!JS_CallFunctionValue(gCx, g, fn, JS::HandleValueArray::empty(), &mb))
        return false;
    if (!mb.isObject()) return false;
    bool shared;
    size_t len;
    JS::GetArrayBufferLengthAndData(&mb.toObject(), &len, &shared, &wasm_memory);
    return wasm_memory != nullptr;
}

// Read a cached global pointer from the JS _gp object.
static uint32_t get_gp(JS::HandleObject gpObj, const char* name) {
    JS::RootedValue val(gCx);
    if (!JS_GetProperty(gCx, gpObj, name, &val) || !val.isNumber()) return 0;
    double d = val.toNumber();
    if (d < 0 || d > 0xFFFFFFFFu) return 0;
    return (uint32_t)d;
}

// ============================================================
// Init window / textures / audio from ROM globals
// ============================================================

static void init_from_globals() {
    if (!wasm_memory) return;

    W     = mem_u32(gptr_width);
    H     = mem_u32(gptr_height);
    BPP   = mem_u32(gptr_bpp);
    SCALE = mem_u32(gptr_scale);
    if (W == 0)     W = 320;
    if (H == 0)     H = 240;
    if (BPP == 0)   BPP = 32;
    if (SCALE == 0) SCALE = 1;

    char title[128];
    mem_str(gptr_title, title, 128);

    if (!window) {
        // First-time window creation with OpenGL 3.0 Core Profile
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

        window = SDL_CreateWindow(
            title[0] ? title : "Untitled",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            W * SCALE, H * SCALE,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
            return;
        }
        gl_ctx = SDL_GL_CreateContext(window);
        if (!gl_ctx) {
            fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
            return;
        }
        SDL_GL_SetSwapInterval(1);
        init_gpu_pipeline();
    } else {
        SDL_SetWindowSize(window, W * SCALE, H * SCALE);
        SDL_SetWindowTitle(window, title[0] ? title : "Untitled");
    }
    set_viewport_with_letterbox();
    init_textures_and_pbos();

    // Open audio device if the ROM requested it
    uint32_t audio_size = mem_u32(gptr_audio_size);
    if (audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq     = mem_u32(gptr_audio_sample_rate);
        if (wanted.freq == 0) wanted.freq = 44100;
        wanted.format   = AUDIO_F32;
        wanted.channels = mem_u32(gptr_audio_channels);
        if (wanted.channels == 0) wanted.channels = 1;
        wanted.samples  = 1024;
        wanted.callback = host_audio_callback;
        wanted.userdata = NULL;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) {
            SDL_PauseAudioDevice(audio_dev, 0);
        } else {
            fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        }
        prev_audio_rate = wanted.freq;
        prev_audio_channels = wanted.channels;
        prev_audio_size = audio_size;
    }
}

// ============================================================
// Detect audio config changes and reinit if needed
// ============================================================

static void check_audio_config_change() {
    uint32_t cur_size = mem_u32(gptr_audio_size);
    if (audio_dev == 0 && cur_size == 0) return; // ROM hasn't requested audio yet

    uint32_t cur_rate     = mem_u32(gptr_audio_sample_rate);
    uint32_t cur_channels = mem_u32(gptr_audio_channels);

    if (cur_rate == 0) cur_rate = 44100;
    if (cur_channels == 0) cur_channels = 1;

    if (cur_rate != prev_audio_rate ||
        cur_channels != prev_audio_channels ||
        cur_size != prev_audio_size) {
        // Close old device and reopen with new settings
        if (audio_dev) SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;

        if (cur_size > 0) {
            SDL_AudioSpec wanted;
            SDL_zero(wanted);
            wanted.freq     = cur_rate;
            wanted.format   = AUDIO_F32;
            wanted.channels = cur_channels;
            wanted.samples  = 1024;
            wanted.callback = host_audio_callback;
            wanted.userdata = NULL;
            audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
            if (audio_dev) {
                SDL_PauseAudioDevice(audio_dev, 0);
            } else {
                fprintf(stderr, "SDL_OpenAudioDevice (reinit): %s\n", SDL_GetError());
            }
            prev_audio_rate = cur_rate;
            prev_audio_channels = cur_channels;
            prev_audio_size = cur_size;
        }
    } else if (cur_size == 0 && audio_dev != 0) {
        // ROM stopped requesting audio
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
        prev_audio_size = 0;
    }
}

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    // ---- Load WASM binary ----
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "Cannot open ROM: %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = (uint8_t*)malloc(sz);
    if (!wasm_data) { fclose(f); fprintf(stderr, "OOM reading ROM\n"); return 1; }
    fread(wasm_data, 1, sz, f);
    fclose(f);

    // ---- Init SpiderMonkey ----
    if (!JS_Init()) {
        fprintf(stderr, "JS_Init failed\n");
        return 1;
    }
    gCx = JS_NewContext(64L * 1024L * 1024L);
    if (!gCx) {
        fprintf(stderr, "JS_NewContext failed\n");
        return 1;
    }
    if (!JS::InitSelfHostedCode(gCx)) {
        fprintf(stderr, "JS::InitSelfHostedCode failed\n");
        return 1;
    }

    const JSClass cls = { "Wagnostic", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps };
    JS::RealmOptions ro;
    JS::RootedObject global(gCx, JS_NewGlobalObject(gCx, &cls, nullptr,
                                                     JS::FireOnNewGlobalHook, ro));
    if (!global) {
        fprintf(stderr, "JS_NewGlobalObject failed\n");
        return 1;
    }
    gGlobal = global;

    // Expose WASM bytes to JS as an ArrayBuffer property
    {
        JSAutoRealm ar(gCx, global);
        auto copy = mozilla::UniquePtr<uint8_t[], JS::FreePolicy>(
            (uint8_t*)JS_malloc(gCx, sz));
        if (!copy) { fprintf(stderr, "OOM allocating WASM copy\n"); return 1; }
        memcpy(copy.get(), wasm_data, sz);
        JS::RootedObject ab(gCx,
            JS::NewArrayBufferWithContents(gCx, sz, std::move(copy)));
        JS::RootedValue av(gCx, JS::ObjectValue(*ab));
        JS_DefineProperty(gCx, global, "__wasmBytes", av, JSPROP_ENUMERATE);
    }
    free(wasm_data);

    // ---- Init SDL ----
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    /* Create the audio buffer mutex after SDL_Init so SDL_CreateMutex
     * is available. We check for NULL in the callback and main loop
     * so an early audio callback (before we get here) just runs without
     * locking instead of crashing. */
    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "SDL_CreateMutex: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // ---- Instantiate WASM and cache global pointers via JS ----
    {
        JSAutoRealm ar(gCx, global);

        // clang-format off
        if (!eval_js(
            // Instantiate the WASM module. The wagnostic protocol has
            // zero host imports — anything the ROM needs (libc, libm) is
            // compiled into the WASM itself.
            "var _b = new Uint8Array(__wasmBytes);\n"
            "var _m = new WebAssembly.Module(_b);\n"
            "var _imports = {};\n"
            "var _inst = new WebAssembly.Instance(_m, _imports);\n"
            "var _e = _inst.exports;\n"
            // Cache every named global's pointer (i32 -> linear memory offset)
            "var _gp = {};\n"
            "var _gn = [\n"
            "  'w_width','w_height','w_bpp','w_scale','w_title','w_vram',\n"
            "  'w_mouse_x','w_mouse_y','w_mouse_buttons','w_mouse_wheel',\n"
            "  'w_keys','w_gamepad_buttons','w_ticks','w_target_fps',\n"
            "  'w_audio_size','w_audio_sample_rate','w_audio_bpp',\n"
            "  'w_audio_channels','w_audio_write','w_audio_read',\n"
            "  'w_audio_buffer',\n"
            "  'w_dirty_count','w_dirty_rects'\n"
            "];\n"
            "for (var i = 0; i < _gn.length; i++) {\n"
            "  var n = _gn[i];\n"
            "  _gp[n] = (_e[n] !== undefined) ? _e[n].value : 0;\n"
            "}\n"
            // Expose callable wrappers to C++
            "__wupdate = function() { return _e.wupdate(); };\n"
            "__getMem  = function() { return _e.memory.buffer; };\n"
        )) {
            fprintf(stderr, "WASM instantiation/setup JS failed\n");
            return 1;
        }
    }

    // Cache global pointers and get initial memory reference
    {
        JSAutoRealm ar(gCx, global);
        if (!refresh_memory()) {
            fprintf(stderr, "Failed to get WASM memory buffer\n");
            return 1;
        }
    }

    // ---- Read cached global pointers from JS _gp into C++ ----
    {
        JSAutoRealm ar(gCx, global);
        JS::RootedObject g(gCx, gGlobal);
        JS::RootedValue gpVal(gCx);
        if (!JS_GetProperty(gCx, g, "_gp", &gpVal) || !gpVal.isObject()) {
            fprintf(stderr, "No _gp object found\n");
            return 1;
        }
        JS::RootedObject gpObj(gCx, &gpVal.toObject());

        gptr_width             = get_gp(gpObj, "w_width");
        gptr_height            = get_gp(gpObj, "w_height");
        gptr_bpp               = get_gp(gpObj, "w_bpp");
        gptr_scale             = get_gp(gpObj, "w_scale");
        gptr_title             = get_gp(gpObj, "w_title");
        gptr_vram              = get_gp(gpObj, "w_vram");
        gptr_mouse_x           = get_gp(gpObj, "w_mouse_x");
        gptr_mouse_y           = get_gp(gpObj, "w_mouse_y");
        gptr_mouse_buttons     = get_gp(gpObj, "w_mouse_buttons");
        gptr_mouse_wheel       = get_gp(gpObj, "w_mouse_wheel");
        gptr_keys              = get_gp(gpObj, "w_keys");
        gptr_gamepad_buttons   = get_gp(gpObj, "w_gamepad_buttons");
        gptr_target_fps        = get_gp(gpObj, "w_target_fps");
        gptr_ticks             = get_gp(gpObj, "w_ticks");
        gptr_audio_size        = get_gp(gpObj, "w_audio_size");
        gptr_audio_sample_rate = get_gp(gpObj, "w_audio_sample_rate");
        gptr_audio_bpp         = get_gp(gpObj, "w_audio_bpp");
        gptr_audio_channels    = get_gp(gpObj, "w_audio_channels");
        gptr_audio_write       = get_gp(gpObj, "w_audio_write");
        gptr_audio_read        = get_gp(gpObj, "w_audio_read");
        gptr_audio_buffer      = get_gp(gpObj, "w_audio_buffer");
        gptr_dirty_count       = get_gp(gpObj, "w_dirty_count");
        gptr_dirty_rects       = get_gp(gpObj, "w_dirty_rects");
    }

    fprintf(stderr, "Globals: vram=%u  w=%u h=%u bpp=%u scale=%u\n",
            gptr_vram, gptr_width, gptr_height, gptr_bpp, gptr_scale);

    // ---- Set up SDL window / GL / audio from initial ROM config ----
    init_from_globals();

    // ============================================================
    // Main loop
    // ============================================================

    bool running = true;
    while (running) {
        if (!wasm_memory) { SDL_Delay(1); continue; }

        // ---- 1. Write input into WASM memory via named globals ----
        mem_u32(gptr_ticks, SDL_GetTicks());

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }

            // Keyboard: USB HID scancodes (SDL scancodes match directly)
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (ev.key.keysym.scancode < 256 && gptr_keys) {
                    wasm_memory[gptr_keys + ev.key.keysym.scancode] =
                        (ev.type == SDL_KEYDOWN) ? 1 : 0;
                }
            }

            // Mouse motion (with letterbox-aware coordinate conversion)
            if (ev.type == SDL_MOUSEMOTION) {
                int rx, ry;
                convert_mouse_coords(ev.motion.x, ev.motion.y, &rx, &ry);
                mem_i32(gptr_mouse_x, rx);
                mem_i32(gptr_mouse_y, ry);
            }

            // Mouse buttons (bit0=L, bit1=R)
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                uint32_t btns = mem_u32(gptr_mouse_buttons);
                if (ev.button.button == SDL_BUTTON_LEFT)
                    btns = (ev.type == SDL_MOUSEBUTTONDOWN)
                        ? (btns | 1) : (btns & ~1u);
                if (ev.button.button == SDL_BUTTON_RIGHT)
                    btns = (ev.type == SDL_MOUSEBUTTONDOWN)
                        ? (btns | 2) : (btns & ~2u);
                mem_u32(gptr_mouse_buttons, btns);
            }

            // Mouse wheel (accumulated per frame, host resets after consumption)
            if (ev.type == SDL_MOUSEWHEEL) {
                mem_i32(gptr_mouse_wheel,
                    mem_i32(gptr_mouse_wheel) + ev.wheel.y);
            }
        }

        if (!running) break;

        // ---- 2. Call wupdate(), exit if 0 ----
        int32_t keep = 1;
        {
            /* Hold the audio mutex for the whole wupdate call so the
             * audio thread can't read the buffer while the ROM's
             * fill_audio is writing to it. See g_audio_mutex. */
            if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
            {
                JSAutoRealm ar(gCx, global);
                JS::RootedObject g(gCx, gGlobal);
                JS::RootedValue fn(gCx);
                if (JS_GetProperty(gCx, g, "__wupdate", &fn) && fn.isObject()) {
                    JS::RootedValue rv(gCx);
                    JS_CallFunctionValue(gCx, g, fn,
                        JS::HandleValueArray::empty(), &rv);
                    if (rv.isInt32()) keep = rv.toInt32();
                }
                // Memory may grow after wupdate(), refresh pointer
                refresh_memory();
            }
            if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
        }
        if (!keep || !wasm_memory) break;

        // ---- 3. Read config AFTER wupdate() (ROM may change it) ----
        {
            uint32_t cur_W     = mem_u32(gptr_width);
            uint32_t cur_H     = mem_u32(gptr_height);
            uint32_t cur_BPP   = mem_u32(gptr_bpp);
            uint32_t cur_SCALE = mem_u32(gptr_scale);
            if (cur_W == 0)     cur_W = 320;
            if (cur_H == 0)     cur_H = 240;
                if (cur_BPP == 0)   cur_BPP = 32;
                if (cur_SCALE == 0) cur_SCALE = 1;
            if (cur_W != prev_W || cur_H != prev_H ||
                cur_BPP != prev_BPP || cur_SCALE != prev_SCALE) {
                W = cur_W; H = cur_H; BPP = cur_BPP; SCALE = cur_SCALE;
                prev_W = W; prev_H = H; prev_BPP = BPP; prev_SCALE = SCALE;
                set_viewport_with_letterbox();
                init_textures_and_pbos();
            }
        }

        // ---- 4. Check audio config changes ----
        check_audio_config_change();

        // ---- 5. Render dirty rectangles ----
        uint32_t dirty_count = mem_u32(gptr_dirty_count);
        if (dirty_count > 0) {
            set_viewport_with_letterbox();

            if (dirty_count == 1 && gptr_dirty_rects) {
                // Fast path: check if single dirty rect covers full screen
                Rect r = mem_rect(gptr_dirty_rects, 0);
                if (r.x == 0 && r.y == 0 &&
                    (uint32_t)r.w == W && (uint32_t)r.h == H) {
                    if (first_frame) {
                        // First frame: direct upload without PBO pipeline
                        uint8_t* vram = wasm_memory + gptr_vram;
                        GLenum fmt = (BPP == 8) ? GL_RED :
                                     (BPP == 16) ? GL_RG : GL_RGBA;
                        glBindTexture(GL_TEXTURE_2D, vram_textures[0]);
                        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H,
                                        fmt, GL_UNSIGNED_BYTE, vram);
                        tex_idx = 1;
                        first_frame = 0;
                        glClear(GL_COLOR_BUFFER_BIT);
                        render_quad(vram_textures[0]);
                        SDL_GL_SwapWindow(window);
                    } else {
                        upload_and_render();
                    }
                } else {
                    // Single partial dirty rect
                    upload_dirty_rect(r);
                    glClear(GL_COLOR_BUFFER_BIT);
                    render_quad(vram_textures[tex_idx]);
                    SDL_GL_SwapWindow(window);
                }
            } else if (gptr_dirty_rects) {
                // Multiple dirty rects: upload each sub-region
                uint32_t count = dirty_count;
                if (count > 32) count = 32; // safety cap (Rect[32] max)
                for (uint32_t i = 0; i < count; i++) {
                    Rect r = mem_rect(gptr_dirty_rects, (int)i);
                    upload_dirty_rect(r);
                }
                glClear(GL_COLOR_BUFFER_BIT);
                render_quad(vram_textures[tex_idx]);
                SDL_GL_SwapWindow(window);
            }
            mem_u32(gptr_dirty_count, 0); // consume after rendering
        }

        // ---- 6. Reset mouse wheel after consumption ----
        mem_i32(gptr_mouse_wheel, 0);

        uint32_t target_fps = gptr_target_fps ? mem_u32(gptr_target_fps) : 0;
        if (target_fps > 0) {
            static uint32_t frame_start = 0;
            uint32_t now = SDL_GetTicks();
            uint32_t elapsed = now - frame_start;
            int32_t delay = (1000 / target_fps) - (int32_t)elapsed;
            if (delay > 0) SDL_Delay((uint32_t)delay);
            frame_start = SDL_GetTicks();
        } else {
            SDL_Delay(1);
        }
    }

    // ---- Cleanup ----
    if (audio_dev) {
        SDL_PauseAudioDevice(audio_dev, 1);
        SDL_CloseAudioDevice(audio_dev);
    }
    glDeleteBuffers(2, pbos);
    glDeleteTextures(3, vram_textures);
    glDeleteProgram(render_program);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    if (gl_ctx) SDL_GL_DeleteContext(gl_ctx);
    if (window) SDL_DestroyWindow(window);

    /* Audio callback has been paused (SDL_CloseAudioDevice was called
     * above), so no other thread holds the mutex. Safe to destroy. */
    if (g_audio_mutex) SDL_DestroyMutex(g_audio_mutex);
    g_audio_mutex = NULL;

    SDL_Quit();

    // Release JS global reference
    gGlobal = nullptr;

    JS_DestroyContext(gCx);
    JS_ShutDown();

    return 0;
}
