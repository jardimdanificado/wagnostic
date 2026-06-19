#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>

#include "wasmtime.h"

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

// Vertex shader
static const char* vs_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "out vec2 TexCoord;\n"
    "void main() { gl_Position = vec4(pos, 0, 1); TexCoord = uv; }\n";

// Fragment shader with GPU pixel format conversion
static const char* fs_src =
    "#version 330 core\n"
    "in vec2 TexCoord; out vec4 FragColor;\n"
    "uniform sampler2D vram; uniform int bpp;\n"
    "void main() {\n"
    "  ivec2 p = ivec2(floor(TexCoord * vec2(textureSize(vram, 0))));\n"
    "  if (bpp == 8) {\n"
    "    uint r = uint(texelFetch(vram, p, 0).r * 255.0);\n"
    "    float R = float((r>>5)&0x07u)/7.0, G = float((r>>2)&0x07u)/7.0, B = float(r&0x03u)/3.0;\n"
    "    FragColor = vec4(R,G,B,1);\n"
    "  } else if (bpp == 16) {\n"
    "    uvec2 r = uvec2(texelFetch(vram, p, 0).rg * 255.0);\n"
    "    uint pk = r.x | (r.y<<8);\n"
    "    float R = float((pk>>11)&0x1Fu)/31.0, G = float((pk>>5)&0x3Fu)/63.0, B = float(pk&0x1Fu)/31.0;\n"
    "    FragColor = vec4(R,G,B,1);\n"
    "  } else { FragColor = texelFetch(vram, p, 0); }\n"
    "}\n";

static GLuint compile_shader(GLenum t, const char* s) {
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, NULL);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(sh, 512, NULL, log);
               fprintf(stderr, "Shader error: %s\n", log); }
    return sh;
}

static void init_gpu(GLuint* tex, GLuint* pbo, GLuint* prog, GLuint* vao,
                     uint32_t w, uint32_t h, uint32_t bpp) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    *prog = glCreateProgram();
    glAttachShader(*prog, vs); glAttachShader(*prog, fs);
    glLinkProgram(*prog);
    glDeleteShader(vs); glDeleteShader(fs);

    float q[] = {-1,-1,0,0, 1,-1,1,0, -1,1,0,1, 1,1,1,1};
    glGenVertexArrays(1, vao);
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*4, 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(2*4));
    glBindVertexArray(0);

    GLenum fmt = (bpp == 8) ? GL_R8 : (bpp == 16) ? GL_RG8 : GL_RGBA8;
    GLenum base = (bpp == 8) ? GL_RED : (bpp == 16) ? GL_RG : GL_RGBA;

    glGenTextures(3, tex);
    for (int i = 0; i < 3; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)fmt, w, h, 0, base, GL_UNSIGNED_BYTE, NULL);
    }

    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    int has_pbo = (major > 2 || (major == 2 && minor >= 1));
    if (has_pbo) {
        glGenBuffers(2, pbo);
        for (int i = 0; i < 2; i++) {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[i]);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, (size_t)w * h * (bpp / 8), NULL, GL_STREAM_DRAW);
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    fprintf(stderr, "  GPU: OpenGL %d.%d | PBO: %s\n",
        major, minor, has_pbo ? "YES (async)" : "NO (direct)");
}

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
    uint8_t* wasm_bytes = malloc(sz);
    fread(wasm_bytes, 1, sz, f);
    fclose(f);

    // Wasmtime setup
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, NULL, NULL);
    wasmtime_context_t* ctx = wasmtime_store_context(store);

    wasmtime_module_t* module = NULL;
    wasmtime_error_t* err = wasmtime_module_new(engine, wasm_bytes, sz, &module);
    if (err) { fprintf(stderr, "ERROR: Compile failed\n"); return 1; }

    wasm_trap_t* trap = NULL;
    wasmtime_instance_t instance;
    err = wasmtime_instance_new(ctx, module, NULL, 0, &instance, &trap);
    if (err) { fprintf(stderr, "ERROR: Instantiate failed\n"); return 1; }

    wasmtime_extern_t ext;
    wasmtime_func_t f_winit, f_wupd;
    int has_winit = 0;
    if (wasmtime_instance_export_get(ctx, &instance, "winit", 5, &ext) && ext.kind == WASMTIME_EXTERN_FUNC)
        { f_winit = ext.of.func; has_winit = 1; }
    if (!wasmtime_instance_export_get(ctx, &instance, "wupdate", 7, &ext) || ext.kind != WASMTIME_EXTERN_FUNC)
        { fprintf(stderr, "ERROR: No wupdate\n"); return 1; }
    f_wupd = ext.of.func;

    if (!wasmtime_instance_export_get(ctx, &instance, "memory", 6, &ext) || ext.kind != WASMTIME_EXTERN_MEMORY)
        { fprintf(stderr, "ERROR: No memory\n"); return 1; }
    wasmtime_memory_t mem_obj = ext.of.memory;
    uint8_t* mem = wasmtime_memory_data(ctx, &mem_obj);

    if (has_winit) {
        wasm_trap_t* t = NULL;
        wasmtime_error_t* e = wasmtime_func_call(ctx, &f_winit, NULL, 0, NULL, 0, &t);
        if (e) { fprintf(stderr, "ERROR: winit\n"); return 1; }
        mem = wasmtime_memory_data(ctx, &mem_obj);
    }

    SystemConfig* sys = (SystemConfig*)mem;
    uint32_t W = sys->width ? sys->width : 320;
    uint32_t H = sys->height ? sys->height : 240;
    uint32_t BPP = sys->bpp ? sys->bpp : 8;
    uint64_t vram_size = (uint64_t)W * H * (BPP / 8);

    // Init SDL + OpenGL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { fprintf(stderr, "ERROR: SDL init\n"); return 1; }
    SDL_Window* win = SDL_CreateWindow("wagnostic", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(win);
    glViewport(0, 0, W, H);

    GLuint textures[3], pbos[2] = {0,0}, program, vao;
    init_gpu(textures, pbos, &program, &vao, W, H, BPP);

    printf("========================================\n");
    printf("  WAGNOSTIC BENCHMARK (wasmtime)\n");
    printf("========================================\n");
    printf("ROM:          %s\n", wasm_path);
    printf("Title:        %s\n", sys->message);
    printf("Resolution:   %ux%u @ %ubpp\n", W, H, BPP);
    printf("VRAM:         %lu KB\n", (unsigned long)(vram_size / 1024));
    printf("Engine:       Wasmtime JIT + GPU (PBO + triple buffer)\n");
    if (sys->audio_size > 0)
        printf("Audio:        %u Hz, %u-bit, %u ch, %u KB buffer\n",
               sys->audio_rate, sys->audio_bpp*8, sys->audio_channels, sys->audio_size/1024);
    else printf("Audio:        none\n");
    printf("Frames:       %d\n", num_frames);
    printf("========================================\n");
    printf("Running benchmark...\n");
    fflush(stdout);

    GLenum upload_fmt = (BPP == 8) ? GL_RED : (BPP == 16) ? GL_RG : GL_RGBA;
    int has_pbo = pbos[0] != 0;
    int frame_idx = 0;

    uint64_t start = now_ns(), total = 0;
    uint64_t min_f = UINT64_MAX, max_f = 0;

    for (int i = 0; i < num_frames; i++) {
        sys = (SystemConfig*)mem;
        sys->ticks = (uint32_t)(i * 16);

        uint64_t fs = now_ns();

        wasm_trap_t* t = NULL;
        wasmtime_func_call(ctx, &f_wupd, NULL, 0, NULL, 0, &t);
        mem = wasmtime_memory_data(ctx, &mem_obj);
        sys = (SystemConfig*)mem;

        int curr = frame_idx % 3;
        int prev = (frame_idx + 2) % 3;

        if (has_pbo) {
            int pbo = frame_idx % 2;
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[pbo]);
            void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr) { memcpy(ptr, mem + 512, vram_size); glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); }
            glBindTexture(GL_TEXTURE_2D, textures[curr]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, upload_fmt, GL_UNSIGNED_BYTE, NULL);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        } else {
            glBindTexture(GL_TEXTURE_2D, textures[curr]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, upload_fmt, GL_UNSIGNED_BYTE, mem + 512);
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "vram"), 0);
        glUniform1i(glGetUniformLocation(program, "bpp"), (GLint)BPP);
        glBindTexture(GL_TEXTURE_2D, textures[prev]);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFinish();

        for (int s = 0; s < 4; s++) sys->signals[s] = 0;
        sys->mouse_wheel = 0;

        uint64_t fe = now_ns();
        uint64_t fn = fe - fs;
        total += fn;
        if (fn < min_f) min_f = fn;
        if (fn > max_f) max_f = fn;
        frame_idx++;

        if ((i+1) % 10 == 0 || i == num_frames-1) {
            printf("\r  Frame %d/%d (%.1f ms/frame avg)", i+1, num_frames, (double)total/(i+1)/1e6);
            fflush(stdout);
        }
    }

    double total_ms = (double)(now_ns() - start) / 1e6;
    double avg_ms = (double)total / num_frames / 1e6;
    double fps = 1000.0 / avg_ms;
    double mpps = (double)W * H * num_frames / (total_ms / 1000.0) / 1e6;
    double bw = (double)vram_size * num_frames / total_ms * 1000.0 / (1024.0*1024.0);

    printf("\n\n");
    printf("========================================\n");
    printf("  RESULTS (wasmtime)\n");
    printf("========================================\n");
    printf("Total time:       %.2f ms\n", total_ms);
    printf("Avg frame time:   %.3f ms\n", avg_ms);
    printf("Min frame time:   %.3f ms\n", (double)min_f/1e6);
    printf("Max frame time:   %.3f ms\n", (double)max_f/1e6);
    printf("Avg FPS:          %.1f\n", fps);
    printf("Pixels/sec:       %.2f MP/s\n", mpps);
    printf("VRAM bandwidth:   %.2f MB/s\n", bw);
    printf("========================================\n");

    glDeleteTextures(3, textures);
    if (has_pbo) glDeleteBuffers(2, pbos);
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &vao);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    free(wasm_bytes);
    return 0;
}
