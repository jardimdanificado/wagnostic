/**
 * Wagnostic SpiderMonkey Host (State Struct Pointer)
 *
 * GPU-accelerated: GLSL 130 shaders, triple-buffered textures,
 * double PBO async upload, SDL2 audio, full input.
 *
 * wupdate() returns an i32 pointer to a WagnosticState struct in
 * WASM linear memory. The host reads/writes that struct directly.
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
/* ================================================================
 * TAR Helpers
 * ================================================================ */

static uint8_t* tar_extract_file(const char* tar_path, const char* target_filename, size_t* out_sz) {
    FILE* f = fopen(tar_path, "rb");
    if (!f) return NULL;
    uint8_t header[512];
    uint8_t* best_data = NULL;
    size_t best_sz = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') break; // End of tar
        char name[101];
        memcpy(name, header, 100);
        name[100] = '\0';
        size_t size = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                size = size * 8 + (header[124+i] - '0');
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

static size_t tar_get_file_size(const char* tar_path, const char* target_filename) {
    FILE* f = fopen(tar_path, "rb");
    if (!f) return 0;
    uint8_t header[512];
    size_t best_sz = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') break;
        char name[101];
        memcpy(name, header, 100);
        name[100] = '\0';
        size_t size = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                size = size * 8 + (header[124+i] - '0');
        }
        if (strcmp(name, target_filename) == 0) {
            best_sz = size;
        }
        long skip = size + ((512 - (size % 512)) % 512);
        fseek(f, skip, SEEK_CUR);
    }
    fclose(f);
    return best_sz;
}

static void tar_append_file(const char* tar_path, const char* target_filename, uint8_t* data, size_t size) {
    FILE* f = fopen(tar_path, "r+b");
    if (!f) return;
    uint8_t header[512];
    long last_good_pos = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') {
            break;
        }
        size_t fsize = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                fsize = fsize * 8 + (header[124+i] - '0');
        }
        long skip = fsize + ((512 - (fsize % 512)) % 512);
        fseek(f, skip, SEEK_CUR);
        last_good_pos = ftell(f);
    }
    fseek(f, last_good_pos, SEEK_SET);
    memset(header, 0, 512);
    strncpy((char*)header, target_filename, 99);
    sprintf((char*)header + 100, "%07o", 0644);
    sprintf((char*)header + 124, "%011zo", size);
    header[135] = ' '; 
    strcpy((char*)header + 257, "ustar  ");
    header[156] = '0'; 
    memset(header + 148, ' ', 8);
    
    uint32_t checksum = 0;
    for (int i = 0; i < 512; i++) {
        checksum += header[i];
    }
    sprintf((char*)header + 148, "%06o", checksum);
    header[154] = '\0';
    header[155] = ' ';
    
    fwrite(header, 1, 512, f);
    fwrite(data, 1, size, f);
    uint8_t zeros[1024] = {0};
    long remainder = (512 - (size % 512)) % 512;
    if (remainder > 0) fwrite(zeros, 1, remainder, f);
    fwrite(zeros, 1, 1024, f); // two empty blocks
    fclose(f);
}


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
// WagnosticState struct (must match wagnostic.h exactly)
// ============================================================

typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    Rect dirty_rects[32];
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t io_load;
    uint32_t io_load_buffer;
    uint32_t io_load_size;
    uint32_t io_save;
    uint32_t io_save_buffer;
    uint32_t io_save_size;
    uint8_t reserved[16];
} WagnosticState;

static_assert(sizeof(WagnosticState) == 1024, "WagnosticState size mismatch — check struct layout");

// ============================================================
// Global state
// ============================================================

static SDL_Window*          window     = NULL;
static SDL_GLContext        gl_ctx     = NULL;
static SDL_AudioDeviceID    audio_dev  = 0;
static uint8_t*             wasm_memory = NULL;
static size_t               wasm_memory_len = 0;

// Latest state pointer (offset into WASM memory)
static uint32_t g_state_ptr = 0;

// Video config
static uint32_t W = 320, H = 240, BPP = 16, SCALE = 1;
static uint32_t prev_W = 0, prev_H = 0, prev_BPP = 0, prev_SCALE = 0;
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

// Audio buffer lock
static SDL_mutex *g_audio_mutex = NULL;

// ============================================================
// Direct memory helpers
// ============================================================

static inline WagnosticState *get_state(void) {
    if (!wasm_memory || g_state_ptr == 0) return NULL;
    if (g_state_ptr + sizeof(WagnosticState) > wasm_memory_len) return NULL;
    return (WagnosticState *)(wasm_memory + g_state_ptr);
}

static inline uint8_t *get_vram(WagnosticState *s) {
    if (!s || s->vram_offset == 0) return NULL;
    return (uint8_t *)s + s->vram_offset;
}

static inline uint8_t *get_audio_buffer(WagnosticState *s) {
    if (!s || s->audio_buffer_offset == 0) return NULL;
    return (uint8_t *)s + s->audio_buffer_offset;
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
    "    } else if (bpp == 24) {\n"
    "        FragColor = vec4(texelFetch(vram, pix, 0).rgb, 1.0);\n"
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

    float quad[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
        -1,  1,  0, 0,
         1,  1,  1, 0,
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
    for (int i = 0; i < 3; i++) {
        if (vram_textures[i]) glDeleteTextures(1, &vram_textures[i]);
        glGenTextures(1, &vram_textures[i]);
        glBindTexture(GL_TEXTURE_2D, vram_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLenum ifmt = (BPP == 8) ? GL_R8 : (BPP == 16) ? GL_RG8 : (BPP == 24) ? GL_RGB8 : GL_RGBA8;
        GLenum base = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : (BPP == 24) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, W, H, 0, base, GL_UNSIGNED_BYTE, NULL);
    }

    if (pbos[0]) glDeleteBuffers(2, pbos);
    glGenBuffers(2, pbos);
    size_t vram_bytes = (size_t)W * H * (BPP == 24 ? 3 : BPP / 8);
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
    WagnosticState *s = get_state();
    uint8_t* vram = get_vram(s);
    if (!vram) return;
    size_t vram_bytes = (size_t)W * H * (BPP == 24 ? 3 : BPP / 8);
    GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : (BPP == 24) ? GL_RGB : GL_RGBA;
    int cur = pbo_idx, prev = 1 - pbo_idx;
    int rtex = (tex_idx + 2) % 3, utex = tex_idx;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[cur]);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) memcpy(ptr, vram, vram_bytes);
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

static void upload_dirty_rect(const Rect& r) {
    int x = r.x, y = r.y, w = r.w, h = r.h;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)W) w = (int)W - x;
    if (y + h > (int)H) h = (int)H - y;
    if (w <= 0 || h <= 0) return;

    WagnosticState *s = get_state();
    uint8_t* vram = get_vram(s);
    if (!vram) return;
    GLenum fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : (BPP == 24) ? GL_RGB : GL_RGBA;
    int bpp_bytes = (BPP == 24) ? 3 : BPP / 8;
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
// Mouse coordinate conversion
// ============================================================

static void convert_mouse_coords(int window_x, int window_y, int* rom_x, int* rom_y) {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    WagnosticState *s = get_state();
    uint32_t rw = s ? s->width  : 320;
    uint32_t rh = s ? s->height : 240;
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
// Audio callback
// ============================================================

static void host_audio_callback(void* userdata, Uint8* stream_ptr, int len_bytes) {
    if (!wasm_memory) return;
    if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);

    WagnosticState *s = get_state();
    uint8_t* audio_buf = get_audio_buffer(s);
    uint32_t r   = s ? s->audio_read  : 0;
    uint32_t w   = s ? s->audio_write : 0;
    uint32_t sz  = s ? s->audio_size  : 0;
    uint32_t bpp = s ? s->audio_bpp   : 0;
    if (sz == 0 || !audio_buf) {
        if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
        return;
    }

    float* stream = (float*)stream_ptr;
    int total_samples = len_bytes / (int)sizeof(float);

    for (int i = 0; i < total_samples; i++) {
        if (r == w) {
            stream[i] = 0.0f;
            continue;
        }
        float sample = 0;
        if (bpp == 1) {
            sample = (audio_buf[r] - 128) / 128.0f;
            r = (r + 1) % sz;
        } else if (bpp == 2) {
            sample = (*(int16_t*)(audio_buf + r)) / 32768.0f;
            r = (r + 2) % sz;
        } else {
            sample = *(float*)(audio_buf + r);
            r = (r + 4) % sz;
        }
        stream[i] = sample;
    }
    if (s) s->audio_read = r;

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

static bool refresh_memory() {
    JS::RootedObject g(gCx, gGlobal);
    JS::RootedValue fn(gCx);
    if (!JS_GetProperty(gCx, g, "__getMem", &fn) || !fn.isObject()) return false;
    JS::RootedValue mb(gCx);
    if (!JS_CallFunctionValue(gCx, g, fn, JS::HandleValueArray::empty(), &mb))
        return false;
    if (!mb.isObject()) return false;
    bool shared;
    JS::GetArrayBufferLengthAndData(&mb.toObject(), &wasm_memory_len, &shared, &wasm_memory);
    return wasm_memory != nullptr;
}

// ============================================================
// Init window / textures / audio from ROM state
// ============================================================

static void init_from_state() {
    WagnosticState *s = get_state();
    if (!s) return;

    W = s->width;   if (W == 0)   W = 320;
    H = s->height;  if (H == 0)   H = 240;
    BPP = s->bpp;   if (BPP == 0) BPP = 32;
    SCALE = s->scale; if (SCALE == 0) SCALE = 1;

    char title[128];
    strncpy(title, s->title, 127);
    title[127] = '\0';

    if (!window) {
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

    uint32_t audio_size = s->audio_size;
    if (audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq     = s->audio_sample_rate ? s->audio_sample_rate : 44100;
        wanted.format   = AUDIO_F32;
        wanted.channels = s->audio_channels ? s->audio_channels : 1;
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
// Detect audio config changes
// ============================================================

static void check_audio_config_change() {
    WagnosticState *s = get_state();
    if (!s) return;

    uint32_t cur_size = s->audio_size;
    if (audio_dev == 0 && cur_size == 0) return;

    uint32_t cur_rate     = s->audio_sample_rate;
    uint32_t cur_channels = s->audio_channels;
    if (cur_rate == 0) cur_rate = 44100;
    if (cur_channels == 0) cur_channels = 1;

    if (cur_rate != prev_audio_rate ||
        cur_channels != prev_audio_channels ||
        cur_size != prev_audio_size) {
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
        fprintf(stderr, "Usage: %s <rom.wasm|rom.wag>\n", argv[0]);
        return 1;
    }
    
    char rom_path[1024];
    strncpy(rom_path, argv[1], sizeof(rom_path)-1);

    // ---- Load WASM/WAG binary ----
    uint8_t* wasm_data = NULL;
    size_t wasm_size = 0;
    int is_tar = 0;

    wasm_data = tar_extract_file(argv[1], "main.wasm", &wasm_size);
    if (wasm_data) {
        is_tar = 1;
    } else {
        FILE* f = fopen(argv[1], "rb");
        if (!f) {
            fprintf(stderr, "Cannot open ROM: %s\n", argv[1]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        wasm_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        wasm_data = (uint8_t*)malloc(wasm_size);
        if (!wasm_data) { fclose(f); fprintf(stderr, "OOM reading ROM\n"); return 1; }
        fread(wasm_data, 1, wasm_size, f);
        fclose(f);
    }

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
            (uint8_t*)JS_malloc(gCx, wasm_size));
        if (!copy) { fprintf(stderr, "OOM allocating WASM copy\n"); return 1; }
        memcpy(copy.get(), wasm_data, wasm_size);
        JS::RootedObject ab(gCx,
            JS::NewArrayBufferWithContents(gCx, wasm_size, std::move(copy)));
        JS::RootedValue av(gCx, JS::ObjectValue(*ab));
        JS_DefineProperty(gCx, global, "__wasmBytes", av, JSPROP_ENUMERATE);
    }
    free(wasm_data);

    // ---- Init SDL ----
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    g_audio_mutex = SDL_CreateMutex();
    if (!g_audio_mutex) {
        fprintf(stderr, "SDL_CreateMutex: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // ---- Instantiate WASM ----
    {
        JSAutoRealm ar(gCx, global);

        if (!eval_js(
            "var _b = new Uint8Array(__wasmBytes);\n"
            "var _m = new WebAssembly.Module(_b);\n"
            "var _imports = {};\n"
            "var _inst = new WebAssembly.Instance(_m, _imports);\n"
            "var _e = _inst.exports;\n"
            "__wupdate = function() { return _e.wupdate(); };\n"
            "__getMem  = function() { return _e.memory.buffer; };\n"
        )) {
            fprintf(stderr, "WASM instantiation/setup JS failed\n");
            return 1;
        }
    }

    // Get initial memory reference
    {
        JSAutoRealm ar(gCx, global);
        if (!refresh_memory()) {
            fprintf(stderr, "Failed to get WASM memory buffer\n");
            return 1;
        }
    }

    // ---- Call wupdate() once to get initial state pointer ----
    {
        JSAutoRealm ar(gCx, global);
        JS::RootedObject g(gCx, gGlobal);
        JS::RootedValue fn(gCx);
        if (g_audio_mutex) SDL_LockMutex(g_audio_mutex);
        if (JS_GetProperty(gCx, g, "__wupdate", &fn) && fn.isObject()) {
            JS::RootedValue rv(gCx);
            JS_CallFunctionValue(gCx, g, fn, JS::HandleValueArray::empty(), &rv);
            if (rv.isInt32()) g_state_ptr = (uint32_t)rv.toInt32();
        }
        refresh_memory();
        if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
    }

    // ---- Set up SDL window / GL / audio from initial ROM config ----
    init_from_state();

    // ============================================================
    // Main loop
    // ============================================================

    bool running = true;
    while (running) {
        if (!wasm_memory) { SDL_Delay(1); continue; }

        // ---- 1. Write input into state struct ----
        {
            WagnosticState *s = get_state();
            if (s) s->ticks = SDL_GetTicks();
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
                break;
            }

            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                WagnosticState *s = get_state();
                if (s && ev.key.keysym.scancode < 256) {
                    s->keys[ev.key.keysym.scancode] =
                        (ev.type == SDL_KEYDOWN) ? 1 : 0;
                }
            }

            if (ev.type == SDL_MOUSEMOTION) {
                int rx, ry;
                convert_mouse_coords(ev.motion.x, ev.motion.y, &rx, &ry);
                WagnosticState *s = get_state();
                if (s) { s->mouse_x = rx; s->mouse_y = ry; }
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                WagnosticState *s = get_state();
                if (s) {
                    if (ev.button.button == SDL_BUTTON_LEFT) {
                        if (ev.type == SDL_MOUSEBUTTONDOWN)
                            s->mouse_buttons |= 1;
                        else
                            s->mouse_buttons &= ~1u;
                    }
                    if (ev.button.button == SDL_BUTTON_RIGHT) {
                        if (ev.type == SDL_MOUSEBUTTONDOWN)
                            s->mouse_buttons |= 2;
                        else
                            s->mouse_buttons &= ~2u;
                    }
                }
            }

            if (ev.type == SDL_MOUSEWHEEL) {
                WagnosticState *s = get_state();
                if (s) s->mouse_wheel += ev.wheel.y;
            }
        }

        if (!running) break;

        // ---- 2. Call wupdate(), exit if 0 ----
        int32_t keep = 1;
        {
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
                refresh_memory();
            }
            if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex);
        }
        if (!keep || !wasm_memory) break;
        g_state_ptr = (uint32_t)keep;

        // ---- 3. Read config AFTER wupdate() ----
        {
            WagnosticState *s = get_state();
            uint32_t cur_W     = s ? s->width  : 320;
            uint32_t cur_H     = s ? s->height : 240;
            uint32_t cur_BPP   = s ? s->bpp    : 32;
            uint32_t cur_SCALE = s ? s->scale  : 1;
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
        {
            WagnosticState *s = get_state();
            if (!s) continue;
            uint32_t dirty_count = s->dirty_count;
            if (dirty_count > 0) {
                set_viewport_with_letterbox();

                if (dirty_count == 1) {
                    Rect r = s->dirty_rects[0];
                    if (r.x == 0 && r.y == 0 &&
                        (uint32_t)r.w == W && (uint32_t)r.h == H) {
                        if (first_frame) {
                            uint8_t* vram = get_vram(s);
                            if (vram) {
                                GLenum fmt = (BPP == 8) ? GL_RED :
                                             (BPP == 16) ? GL_RG :
                                             (BPP == 24) ? GL_RGB : GL_RGBA;
                                glBindTexture(GL_TEXTURE_2D, vram_textures[0]);
                                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H,
                                                fmt, GL_UNSIGNED_BYTE, vram);
                                tex_idx = 1;
                                first_frame = 0;
                                glClear(GL_COLOR_BUFFER_BIT);
                                render_quad(vram_textures[0]);
                                SDL_GL_SwapWindow(window);
                            }
                        } else {
                            upload_and_render();
                        }
                    } else {
                        upload_dirty_rect(r);
                        glClear(GL_COLOR_BUFFER_BIT);
                        render_quad(vram_textures[tex_idx]);
                        SDL_GL_SwapWindow(window);
                    }
                } else {
                    uint32_t count = dirty_count;
                    if (count > 32) count = 32;
                    for (uint32_t i = 0; i < count; i++) {
                        upload_dirty_rect(s->dirty_rects[i]);
                    }
                    glClear(GL_COLOR_BUFFER_BIT);
                    render_quad(vram_textures[tex_idx]);
                    SDL_GL_SwapWindow(window);
                }
                s->dirty_count = 0;
            }
        }

        // ---- 6. Process IO Streams ----
        {
            WagnosticState *s = get_state();
            if (s && is_tar) {
                // Process Load
                if (s->io_load && s->io_load < wasm_memory_len) {
                    const char *path = (const char *)(wasm_memory + s->io_load);
                    size_t file_sz = tar_get_file_size(rom_path, path);
                    if (file_sz > 0) {
                        if (s->io_load_buffer == 0) {
                            s->io_load_size = (uint32_t)file_sz;
                        } else if (s->io_load_buffer + file_sz <= wasm_memory_len) {
                            size_t exact_sz = 0;
                            uint8_t *data = tar_extract_file(rom_path, path, &exact_sz);
                            if (data) {
                                memcpy(wasm_memory + s->io_load_buffer, data, exact_sz);
                                free(data);
                            }
                        }
                    } else {
                        if (s->io_load_buffer == 0) s->io_load_size = 0;
                    }
                    s->io_load = 0;
                }

                // Process Save
                if (s->io_save && s->io_save < wasm_memory_len) {
                    const char *path = (const char *)(wasm_memory + s->io_save);
                    if (s->io_save_buffer > 0 && s->io_save_buffer + s->io_save_size <= wasm_memory_len) {
                        tar_append_file(rom_path, path, wasm_memory + s->io_save_buffer, s->io_save_size);
                        printf("Saved to %s\n", rom_path);
                    }
                    s->io_save = 0;
                }
            }
        }

        // ---- 7. Reset mouse wheel after consumption ----
        {
            WagnosticState *s = get_state();
            if (s) s->mouse_wheel = 0;
        }

        {
            WagnosticState *s = get_state();
            uint32_t target_fps = s ? s->target_fps : 0;
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
    if (g_audio_mutex) SDL_DestroyMutex(g_audio_mutex);
    g_audio_mutex = NULL;
    SDL_Quit();

    gGlobal = nullptr;
    JS_DestroyContext(gCx);
    JS_ShutDown();

    return 0;
}
