typedef struct { int x, y, w, h; } Rect;
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



typedef struct {
    uint32_t width, height, scale;
    char title[128];
    uint32_t dirty_rects;
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
    uint32_t audio_chunk_samples, audio_volume, audio_paused;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint8_t reserved[504];
} WagnosticState;

static_assert(sizeof(WagnosticState) == 1024, "WagnosticState size mismatch — check struct layout");

static inline uint32_t compute_bpp(WagnosticState* s) {
    if (!s) return 32;
    uint32_t max_bit = 0;
    auto check = [&](uint32_t bits, uint32_t shift) {
        if (bits && shift + bits > max_bit) max_bit = shift + bits;
    };
    check(s->r_bits, s->r_shift);
    check(s->g_bits, s->g_shift);
    check(s->b_bits, s->b_shift);
    check(s->a_bits, s->a_shift);
    check(s->x_bits, s->x_shift);
    if (!max_bit)   return 32;
    if (max_bit <= 1)  return 1;
    if (max_bit <= 2)  return 2;
    if (max_bit <= 4)  return 4;
    if (max_bit <= 8)  return 8;
    if (max_bit <= 16) return 16;
    if (max_bit <= 24) return 24;
    if (max_bit <= 32) return 32;
    return 64;
}



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
static int tex_idx = 0, pbo_idx = 0;
static GLuint vao = 0, vbo = 0;

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
    "void main() {\n"
    "    ivec2 pix = ivec2(floor(TexCoord * vec2(textureSize(vram, 0))));\n"
    "    FragColor = texelFetch(vram, pix, 0);\n"
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

    // Alpha blending: src_alpha over black background (matches web emulator behavior)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    if (pbos[0]) glDeleteBuffers(2, pbos);
    glGenBuffers(2, pbos);
    size_t vram_bytes = (size_t)W * H * 4;
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, vram_bytes, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    tex_idx = 0;
    pbo_idx = 0;
}

static void render_quad(GLuint tex_id) {
    glUseProgram(render_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(render_program, "vram"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
}

typedef enum {
    FMT_GENERIC = 0,
    FMT_RGBA8888_LE,
    FMT_BGRA8888_LE,
    FMT_RGBX8888_LE,
    FMT_BGRX8888_LE,
    FMT_RGB888,
    FMT_BGR888,
    FMT_RGB565,
    FMT_BGR565,
    FMT_RGB555,
    FMT_BGR555,
    FMT_RGB444,
    FMT_RGBA4444,
    FMT_ARGB4444,
    FMT_RGB333,
    FMT_RGB332,
    FMT_RGB222,
    FMT_RGBA2222,
    FMT_RGB111,
    FMT_GRAY8,
    FMT_RGB666,
    FMT_MONO1,
    FMT_MONO2,
    FMT_MONO4
} WagnosticPixelFormat;

static WagnosticPixelFormat detect_pixel_format(WagnosticState *s, uint32_t BPP,
                                               uint32_t *r_b_out, uint32_t *r_s_out,
                                               uint32_t *g_b_out, uint32_t *g_s_out,
                                               uint32_t *b_b_out, uint32_t *b_s_out,
                                               uint32_t *a_b_out, uint32_t *a_s_out) {
    uint32_t r_b = s ? s->r_bits : 0;
    uint32_t r_s = s ? s->r_shift : 0;
    uint32_t g_b = s ? s->g_bits : 0;
    uint32_t g_s = s ? s->g_shift : 0;
    uint32_t b_b = s ? s->b_bits : 0;
    uint32_t b_s = s ? s->b_shift : 0;
    uint32_t a_b = s ? s->a_bits : 0;
    uint32_t a_s = s ? s->a_shift : 0;
    uint32_t x_b = s ? s->x_bits : 0;

    if (!r_b && !g_b && !b_b && !a_b && !x_b) {
        if (BPP == 32) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; a_b = 8; a_s = 24; }
        else if (BPP == 24) { r_b = 8; r_s = 16; g_b = 8; g_s = 8; b_b = 8; b_s = 0; }
        else if (BPP == 16) { r_b = 5; r_s = 11; g_b = 6; g_s = 5; b_b = 5; b_s = 0; }
        else if (BPP == 8)  { r_b = 3; r_s = 5;  g_b = 3; g_s = 2; b_b = 2; b_s = 0; }
        else if (BPP == 4 || BPP == 2 || BPP == 1) { a_b = BPP; a_s = 0; }
    }

    if (r_b_out) *r_b_out = r_b; if (r_s_out) *r_s_out = r_s;
    if (g_b_out) *g_b_out = g_b; if (g_s_out) *g_s_out = g_s;
    if (b_b_out) *b_b_out = b_b; if (b_s_out) *b_s_out = b_s;
    if (a_b_out) *a_b_out = a_b; if (a_s_out) *a_s_out = a_s;

    if (BPP == 1) return FMT_MONO1;
    if (BPP == 2) return FMT_MONO2;
    if (BPP == 4) return FMT_MONO4;

    if (BPP == 32) {
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16 && a_b == 8 && a_s == 24) return FMT_RGBA8888_LE;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0  && a_b == 8 && a_s == 24) return FMT_BGRA8888_LE;
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16 && a_b == 0) return FMT_RGBX8888_LE;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0  && a_b == 0) return FMT_BGRX8888_LE;
    }

    if (BPP == 24) {
        if (r_b == 8 && r_s == 0  && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 16) return FMT_RGB888;
        if (r_b == 8 && r_s == 16 && g_b == 8 && g_s == 8 && b_b == 8 && b_s == 0)  return FMT_BGR888;
    }

    if (BPP == 16) {
        if (r_b == 5 && r_s == 11 && g_b == 6 && g_s == 5  && b_b == 5 && b_s == 0  && a_b == 0) return FMT_RGB565;
        if (r_b == 5 && r_s == 0  && g_b == 6 && g_s == 5  && b_b == 5 && b_s == 11 && a_b == 0) return FMT_BGR565;
        if (r_b == 5 && r_s == 10 && g_b == 5 && g_s == 5  && b_b == 5 && b_s == 0  && (a_b == 0 || a_b == 1)) return FMT_RGB555;
        if (r_b == 5 && r_s == 0  && g_b == 5 && g_s == 5  && b_b == 5 && b_s == 10 && (a_b == 0 || a_b == 1)) return FMT_BGR555;
        if (r_b == 4 && r_s == 8  && g_b == 4 && g_s == 4  && b_b == 4 && b_s == 0  && a_b == 0) return FMT_RGB444;
        if (r_b == 4 && r_s == 12 && g_b == 4 && g_s == 8  && b_b == 4 && b_s == 4  && a_b == 4 && a_s == 0) return FMT_RGBA4444;
        if (r_b == 4 && r_s == 8  && g_b == 4 && g_s == 4  && b_b == 4 && b_s == 0  && a_b == 4 && a_s == 12) return FMT_ARGB4444;
        if (r_b == 3 && r_s == 6  && g_b == 3 && g_s == 3  && b_b == 3 && b_s == 0  && a_b == 0) return FMT_RGB333;
    }

    if (BPP == 8) {
        if (r_b == 3 && r_s == 5 && g_b == 3 && g_s == 2 && b_b == 2 && b_s == 0 && a_b == 0) return FMT_RGB332;
        if (r_b == 2 && r_s == 4 && g_b == 2 && g_s == 2 && b_b == 2 && b_s == 0 && a_b == 0) return FMT_RGB222;
        if (r_b == 2 && r_s == 6 && g_b == 2 && g_s == 4 && b_b == 2 && b_s == 2 && a_b == 2 && a_s == 0) return FMT_RGBA2222;
        if (r_b == 1 && r_s == 2 && g_b == 1 && g_s == 1 && b_b == 1 && b_s == 0 && a_b == 0) return FMT_RGB111;
        if (r_b == 0 && g_b == 0 && b_b == 0) return FMT_GRAY8;
    }

    if ((BPP == 24 || BPP == 32) && r_b == 6 && r_s == 12 && g_b == 6 && g_s == 6 && b_b == 6 && b_s == 0 && a_b == 0) {
        return FMT_RGB666;
    }

    return FMT_GENERIC;
}

static void unpack_rect_cpu(WagnosticState *s, uint8_t* vram, uint32_t* dst, int rx, int ry, int rw, int rh) {
    uint32_t BPP = compute_bpp(s);
    uint32_t r_b = 0, r_s = 0, g_b = 0, g_s = 0, b_b = 0, b_s = 0, a_b = 0, a_s = 0;
    WagnosticPixelFormat fmt = detect_pixel_format(s, BPP, &r_b, &r_s, &g_b, &g_s, &b_b, &b_s, &a_b, &a_s);

    switch (fmt) {
        case FMT_RGBA8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                memcpy(dst_line, src, rw * sizeof(uint32_t));
            }
            break;
        }
        case FMT_BGRA8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    dst_line[x] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
                }
            }
            break;
        }
        case FMT_RGBX8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    dst_line[x] = 0xFF000000 | (src[x] & 0x00FFFFFF);
                }
            }
            break;
        }
        case FMT_BGRX8888_LE: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint32_t *src = (const uint32_t *)(vram + (y * W + rx) * 4);
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    dst_line[x] = 0xFF000000 | (px & 0x0000FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
                }
            }
            break;
        }
        case FMT_RGB888: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + (y * W + rx) * 3;
                for (int x = 0; x < rw; x++) {
                    dst_line[x] = 0xFF000000 | (src[2] << 16) | (src[1] << 8) | src[0];
                    src += 3;
                }
            }
            break;
        }
        case FMT_BGR888: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + (y * W + rx) * 3;
                for (int x = 0; x < rw; x++) {
                    dst_line[x] = 0xFF000000 | (src[0] << 16) | (src[1] << 8) | src[2];
                    src += 3;
                }
            }
            break;
        }
        case FMT_RGB565: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t b = px & 0x1F;        b = (b << 3) | (b >> 2);
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_BGR565: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t b = (px >> 11) & 0x1F; b = (b << 3) | (b >> 2);
                    uint32_t g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t r = px & 0x1F;        r = (r << 3) | (r >> 2);
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB555: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 10) & 0x1F; r = (r << 3) | (r >> 2);
                    uint32_t g = (px >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
                    uint32_t b = px & 0x1F;        b = (b << 3) | (b >> 2);
                    uint32_t a = (a_b == 1 && !(px & (1 << a_s))) ? 0 : 255;
                    dst_line[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_BGR555: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t b = (px >> 10) & 0x1F; b = (b << 3) | (b >> 2);
                    uint32_t g = (px >> 5)  & 0x1F; g = (g << 3) | (g >> 2);
                    uint32_t r = px & 0x1F;        r = (r << 3) | (r >> 2);
                    uint32_t a = (a_b == 1 && !(px & (1 << a_s))) ? 0 : 255;
                    dst_line[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 8) & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 4) & 0x0F; g = (g << 4) | g;
                    uint32_t b = px & 0x0F;        b = (b << 4) | b;
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGBA4444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 12) & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 8)  & 0x0F; g = (g << 4) | g;
                    uint32_t b = (px >> 4)  & 0x0F; b = (b << 4) | b;
                    uint32_t a = px & 0x0F;         a = (a << 4) | a;
                    dst_line[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_ARGB4444: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t a = (px >> 12) & 0x0F; a = (a << 4) | a;
                    uint32_t r = (px >> 8)  & 0x0F; r = (r << 4) | r;
                    uint32_t g = (px >> 4)  & 0x0F; g = (g << 4) | g;
                    uint32_t b = px & 0x0F;         b = (b << 4) | b;
                    dst_line[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB333: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint16_t *src = (const uint16_t *)(vram) + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 6) & 7; r = (r << 5) | (r << 2) | (r >> 1);
                    uint32_t g = (px >> 3) & 7; g = (g << 5) | (g << 2) | (g >> 1);
                    uint32_t b = px & 7;        b = (b << 5) | (b << 2) | (b >> 1);
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB332: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 5) & 7; r = (r << 5) | (r << 2) | (r >> 1);
                    uint32_t g = (px >> 2) & 7; g = (g << 5) | (g << 2) | (g >> 1);
                    uint32_t b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB222: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 4) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
                    uint32_t g = (px >> 2) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
                    uint32_t b = px & 3;        b = (b << 6) | (b << 4) | (b << 2) | b;
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGBA2222: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px >> 6) & 3; r = (r << 6) | (r << 4) | (r << 2) | r;
                    uint32_t g = (px >> 4) & 3; g = (g << 6) | (g << 4) | (g << 2) | g;
                    uint32_t b = (px >> 2) & 3; b = (b << 6) | (b << 4) | (b << 2) | b;
                    uint32_t a = px & 3;        a = (a << 6) | (a << 4) | (a << 2) | a;
                    dst_line[x] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_RGB111: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t px = src[x];
                    uint32_t r = (px & 4) ? 255 : 0;
                    uint32_t g = (px & 2) ? 255 : 0;
                    uint32_t b = (px & 1) ? 255 : 0;
                    dst_line[x] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_GRAY8: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                const uint8_t *src = vram + y * W + rx;
                for (int x = 0; x < rw; x++) {
                    uint32_t lum = src[x];
                    dst_line[x] = 0xFF000000 | (lum << 16) | (lum << 8) | lum;
                }
            }
            break;
        }
        case FMT_RGB666: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint32_t px = (BPP == 32) ? ((uint32_t*)vram)[idx] : (vram[idx*3] | (vram[idx*3+1]<<8) | (vram[idx*3+2]<<16));
                    uint32_t r = (px >> 12) & 0x3F; r = (r << 2) | (r >> 4);
                    uint32_t g = (px >> 6)  & 0x3F; g = (g << 2) | (g >> 4);
                    uint32_t b = px & 0x3F;         b = (b << 2) | (b >> 4);
                    dst_line[x - rx] = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
        case FMT_MONO1: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 8];
                    uint8_t bit = (b_val >> (7 - (idx % 8))) & 1;
                    dst_line[x - rx] = bit ? 0xFFFFFFFF : 0xFF000000;
                }
            }
            break;
        }
        case FMT_MONO2: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 4];
                    uint8_t val = (b_val >> (6 - (idx % 4) * 2)) & 0x03;
                    val = (val << 6) | (val << 4) | (val << 2) | val;
                    dst_line[x - rx] = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
            break;
        }
        case FMT_MONO4: {
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                for (int x = rx; x < rx + rw; x++) {
                    size_t idx = y * W + x;
                    uint8_t b_val = vram[idx / 2];
                    uint8_t val = (idx % 2 == 0) ? (b_val >> 4) : (b_val & 0x0F);
                    val = (val << 4) | val;
                    dst_line[x - rx] = 0xFF000000 | (val << 16) | (val << 8) | val;
                }
            }
            break;
        }
        default: {
            int is_grayscale = (!r_b && !g_b && !b_b && a_b);
            for (int y = ry; y < ry + rh; y++) {
                uint32_t *dst_line = dst + (y - ry) * rw;
                for (int x = rx; x < rx + rw; x++) {
                    uint32_t idx = y * W + x;
                    uint64_t px = 0;
                    if (BPP == 64) px = ((uint64_t*)vram)[idx];
                    else if (BPP == 32) px = ((uint32_t*)vram)[idx];
                    else if (BPP == 24) {
                        uint8_t *p = vram + idx * 3;
                        px = p[0] | (p[1] << 8) | (p[2] << 16);
                    }
                    else if (BPP == 16) px = ((uint16_t*)vram)[idx];
                    else if (BPP == 8) px = vram[idx];
                    else if (BPP == 4) {
                        uint8_t b_val = vram[idx / 2];
                        px = (idx % 2 == 0) ? (b_val >> 4) : (b_val & 0x0F);
                    }
                    else if (BPP == 2) {
                        uint8_t b_val = vram[idx / 4];
                        px = (b_val >> (6 - (idx % 4) * 2)) & 0x03;
                    }
                    else if (BPP == 1) {
                        uint8_t b_val = vram[idx / 8];
                        px = (b_val >> (7 - (idx % 8))) & 1;
                    }

                    uint32_t r = 0, g = 0, b = 0, a = 255;
                    if (is_grayscale) {
                        uint32_t lum = 0;
                        if (a_b > 0) lum = (uint32_t)(((px >> a_s) & ((1ULL << a_b) - 1)) * 255 / ((1ULL << a_b) - 1));
                        else lum = px ? 255 : 0;
                        r = g = b = lum;
                        a = 255;
                    } else {
                        if (r_b) r = (uint32_t)(((px >> r_s) & ((1ULL << r_b) - 1)) * 255 / ((1ULL << r_b) - 1));
                        if (g_b) g = (uint32_t)(((px >> g_s) & ((1ULL << g_b) - 1)) * 255 / ((1ULL << g_b) - 1));
                        if (b_b) b = (uint32_t)(((px >> b_s) & ((1ULL << b_b) - 1)) * 255 / ((1ULL << b_b) - 1));
                        if (a_b) a = (uint32_t)(((px >> a_s) & ((1ULL << a_b) - 1)) * 255 / ((1ULL << a_b) - 1));
                    }
                    dst_line[x - rx] = (a << 24) | (b << 16) | (g << 8) | r;
                }
            }
            break;
        }
    }
}

static void upload_and_render() {
    WagnosticState *s = get_state();
    uint8_t* vram = get_vram(s);
    if (!vram) return;
    int cur = pbo_idx, prev = 1 - pbo_idx;
    int rtex = (tex_idx + 2) % 3, utex = tex_idx;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[cur]);
    void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr) {
        unpack_rect_cpu(s, vram, (uint32_t*)ptr, 0, 0, W, H);
    }
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[prev]);
    glBindTexture(GL_TEXTURE_2D, vram_textures[utex]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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
    
    uint32_t *tmp = new uint32_t[w * h];
    unpack_rect_cpu(s, vram, tmp, x, y, w, h);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, vram_textures[tex_idx]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    delete[] tmp;
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
    if (sz == 0 || !audio_buf || (s && s->audio_paused)) {
        memset(stream_ptr, 0, len_bytes);
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

    if (s && s->audio_volume > 0 && s->audio_volume < 255) {
        float vol = (float)s->audio_volume / 255.0f;
        for (int i = 0; i < total_samples; i++) stream[i] *= vol;
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
    BPP = compute_bpp(s);
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
        wanted.samples  = (s && s->audio_chunk_samples >= 128) ? s->audio_chunk_samples : 512;
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
            wanted.samples  = (s && s->audio_chunk_samples >= 128) ? s->audio_chunk_samples : 512;
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
            uint32_t cur_BPP   = compute_bpp(s);
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
            uint32_t dirty_ptr = s->dirty_rects;
            if (dirty_ptr && dirty_ptr + 4 <= (uint32_t)wasm_memory_len) {
                uint32_t dirty_count = *(uint32_t*)(wasm_memory + dirty_ptr);
                Rect* rects = (Rect*)(wasm_memory + dirty_ptr + 4);
                if (dirty_count > 0) {
                    set_viewport_with_letterbox();

                    if (dirty_count == 1) {
                        Rect r = rects[0];
                        if (r.x == 0 && r.y == 0 &&
                            (uint32_t)r.w == W && (uint32_t)r.h == H) {
                            upload_and_render();
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
                            upload_dirty_rect(rects[i]);
                        }
                        glClear(GL_COLOR_BUFFER_BIT);
                        render_quad(vram_textures[tex_idx]);
                        SDL_GL_SwapWindow(window);
                    }
                    s->dirty_rects = 0;
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
