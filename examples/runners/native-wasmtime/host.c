#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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

static SDL_Window* window = NULL;
static SDL_GLContext gl_ctx = NULL;
static SDL_AudioDeviceID audio_dev = 0;
static GLuint render_program = 0;
static GLuint vao = 0, vbo = 0;
static uint32_t W=320, H=240, BPP=8, SCALE=1;
static uint8_t* mem_base = NULL;

static GLuint vram_textures[3];
static GLuint pbos[2];
static int tex_idx = 0;
static int pbo_idx = 0;
static int first_frame = 1;
static GLint bpp_uniform_loc = -1;

static wasmtime_context_t* wasm_ctx = NULL;
static wasmtime_instance_t wasm_instance;
static wasmtime_func_t func_winit, func_wupd;
static wasmtime_memory_t wasm_mem;

static void convert_mouse_coords(int window_x, int window_y, int* rom_x, int* rom_y) {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    
    // Calculate destination rectangle (same as rendering)
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
    
    // Convert window coordinates to ROM coordinates
    float scale_x = (float)W / dst.w;
    float scale_y = (float)H / dst.h;
    
    *rom_x = (int)((window_x - dst.x) * scale_x);
    *rom_y = (int)((window_y - dst.y) * scale_y);
    
    // Clamp to ROM bounds
    if (*rom_x < 0) *rom_x = 0;
    if (*rom_x >= W) *rom_x = W - 1;
    if (*rom_y < 0) *rom_y = 0;
    if (*rom_y >= H) *rom_y = H - 1;
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
        -1,-1, 0,1,  1,-1, 1,1,  -1,1, 0,0,  1,1, 1,0
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

static void set_viewport_with_letterbox() {
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);
    
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    
    // Calculate destination rectangle (same as mouse conversion)
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

static void init_sdl_from_header() {
    if (!mem_base) return;
    SystemConfig* sys = (SystemConfig*)mem_base;

    W = sys->width; H = sys->height; BPP = sys->bpp; SCALE = sys->scale;
    if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 8; if (SCALE == 0) SCALE = 1;

    if (!window) {
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic Wasmtime",
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

    set_viewport_with_letterbox();
    init_textures_and_pbos();

    if (sys->audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = sys->audio_rate ? sys->audio_rate : 44100;
        wanted.format = AUDIO_F32;
        wanted.channels = sys->audio_channels ? sys->audio_channels : 1;
        wanted.samples = 1024;
        wanted.callback = (void(*)(void*,Uint8*,int))&SDL_memset;
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }
}

static int find_export_func(const char* name, wasmtime_func_t* func) {
    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(wasm_ctx, &wasm_instance, name, strlen(name), &ext))
        return 0;
    if (ext.kind != WASMTIME_EXTERN_FUNC) return 0;
    *func = ext.of.func;
    return 1;
}

static int find_memory(wasmtime_memory_t* memory) {
    wasmtime_extern_t ext;
    if (!wasmtime_instance_export_get(wasm_ctx, &wasm_instance, "memory", 6, &ext))
        return 0;
    if (ext.kind != WASMTIME_EXTERN_MEMORY) return 0;
    *memory = ext.of.memory;
    return 1;
}

static wasmtime_error_t* call_func(wasmtime_func_t* func) {
    wasm_trap_t* trap = NULL;
    return wasmtime_func_call(wasm_ctx, func, NULL, 0, NULL, 0, &trap);
}

static void upload_and_render() {
    uint8_t* vram = mem_base + 512;
    size_t vram_bytes = (size_t)W * H * (BPP / 8);

    GLenum fmt;
    if (BPP == 8)       fmt = GL_RED;
    else if (BPP == 16) fmt = GL_RG;
    else                fmt = GL_RGBA;

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
    if (argc < 2) {
        printf("Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("Failed to open ROM"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_bytes = malloc(sz);
    fread(wasm_bytes, 1, sz, f);
    fclose(f);

    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, NULL, NULL);
    wasm_ctx = wasmtime_store_context(store);

    wasmtime_module_t* module = NULL;
    wasmtime_error_t* error = wasmtime_module_new(engine, wasm_bytes, sz, &module);
    if (error) { fprintf(stderr, "ERROR: Compile failed\n"); return 1; }

    wasm_trap_t* trap = NULL;
    error = wasmtime_instance_new(wasm_ctx, module, NULL, 0, &wasm_instance, &trap);
    if (error) { fprintf(stderr, "ERROR: Instantiate failed\n"); return 1; }

    int found_winit = find_export_func("winit", &func_winit);
    int found_wupd = find_export_func("wupdate", &func_wupd);
    int found_mem = find_memory(&wasm_mem);
    if (!found_mem) { fprintf(stderr, "ERROR: ROM has no exported memory\n"); return 1; }

    mem_base = wasmtime_memory_data(wasm_ctx, &wasm_mem);

    if (found_winit) {
        error = call_func(&func_winit);
        if (error) { fprintf(stderr, "ERROR: winit failed\n"); return 1; }
        mem_base = wasmtime_memory_data(wasm_ctx, &wasm_mem);
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "ERROR: SDL init failed\n");
        return 1;
    }
    init_sdl_from_header();

    free(wasm_bytes);
    wasmtime_module_delete(module);

    int running = 1;
    while (running) {
        SystemConfig* sys = (SystemConfig*)mem_base;
        sys->ticks = SDL_GetTicks();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (ev.key.keysym.scancode < 256)
                    sys->keys[ev.key.keysym.scancode] = (ev.type == SDL_KEYDOWN);
            }
            if (ev.type == SDL_MOUSEMOTION) {
                convert_mouse_coords(ev.motion.x, ev.motion.y, &sys->mouse_x, &sys->mouse_y);
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                int b = ev.button.button;
                if (b == SDL_BUTTON_LEFT) sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons|1) : (sys->mouse_buttons&~1);
                if (b == SDL_BUTTON_RIGHT) sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons|2) : (sys->mouse_buttons&~2);
            }
            if (ev.type == SDL_MOUSEWHEEL) sys->mouse_wheel += ev.wheel.y;
        }

        if (found_wupd) {
            error = call_func(&func_wupd);
            if (error) { running = 0; continue; }
            mem_base = wasmtime_memory_data(wasm_ctx, &wasm_mem);
        }

        sys = (SystemConfig*)mem_base;

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
                uint8_t* vram = mem_base + 512;
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

    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    glDeleteBuffers(2, pbos);
    glDeleteTextures(3, vram_textures);
    glDeleteProgram(render_program);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    return 0;
}
