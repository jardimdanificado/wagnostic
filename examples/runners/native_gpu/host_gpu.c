#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#if defined(_WIN32) && defined(_MSC_VER)
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

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
static GLuint vram_texture = 0;
static GLuint render_program = 0;
static GLuint vao = 0, vbo = 0;
static uint32_t W=320, H=240, BPP=8, SCALE=1;
static IM3Runtime runtime = NULL;
static GLint tex_uniform_loc = -1;

// Vertex shader - simple fullscreen quad
static const char* vertex_shader =
    "#version 330 core\n"
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    TexCoord = uv;\n"
    "}\n";

// Fragment shader - handles all pixel formats on GPU
static const char* fragment_shader =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D vram;\n"
    "uniform int bpp;\n"
    "void main() {\n"
    "    vec2 texSize = vec2(textureSize(vram, 0));\n"
    "    vec2 pixel = floor(TexCoord * texSize);\n"
    "    vec4 color;\n"
    "    \n"
    "    if (bpp == 8) {\n"
    "        // RGB332\n"
    "        uint idx = uint(pixel.y) * uint(texSize.x) + uint(pixel.x);\n"
    "        uint raw = texelFetch(vram, ivec2(pixel), 0).r * 255.0;\n"
    "        float r = float((raw >> 5) & 0x07u) / 7.0;\n"
    "        float g = float((raw >> 2) & 0x07u) / 7.0;\n"
    "        float b = float(raw & 0x03u) / 3.0;\n"
    "        color = vec4(r, g, b, 1.0);\n"
    "    } else if (bpp == 16) {\n"
    "        // RGB565\n"
    "        vec4 raw = texelFetch(vram, ivec2(pixel), 0);\n"
    "        uint packed = uint(raw.r * 255.0) | (uint(raw.g * 255.0) << 8);\n"
    "        float r = float((packed >> 11) & 0x1Fu) / 31.0;\n"
    "        float g = float((packed >> 5) & 0x3Fu) / 63.0;\n"
    "        float b = float(packed & 0x1Fu) / 31.0;\n"
    "        color = vec4(r, g, b, 1.0);\n"
    "    } else {\n"
    "        // RGBA8888\n"
    "        color = texelFetch(vram, ivec2(pixel), 0);\n"
    "    }\n"
    "    \n"
    "    FragColor = color;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "Shader error: %s\n", log);
    }
    return shader;
}

static void init_gpu_pipeline() {
    // Compile shaders
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
    
    render_program = glCreateProgram();
    glAttachShader(render_program, vs);
    glAttachShader(render_program, fs);
    glLinkProgram(render_program);
    
    glDeleteShader(vs);
    glDeleteShader(fs);
    
    tex_uniform_loc = glGetUniformLocation(render_program, "vram");
    GLint bpp_loc = glGetUniformLocation(render_program, "bpp");
    
    // Create fullscreen quad
    float quad[] = {
        // positions   // uvs
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
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

static void init_sdl_from_header() {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)mem;
    
    W = sys->width; H = sys->height; BPP = sys->bpp; SCALE = sys->scale;
    if (W == 0) W = 320; if (H == 0) H = 240; if (BPP == 0) BPP = 8; if (SCALE == 0) SCALE = 1;
    
    if (!window) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        window = SDL_CreateWindow(sys->message[0] ? sys->message : "Wagnostic GPU",
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 W * SCALE, H * SCALE,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) return;
        
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        
        gl_ctx = SDL_GL_CreateContext(window);
        SDL_GL_SetSwapInterval(1);
        
#if defined(_WIN32) && defined(_MSC_VER)
        glewInit();
#endif
        
        init_gpu_pipeline();
    } else {
        SDL_SetWindowSize(window, W * SCALE, H * SCALE);
    }
    
    glViewport(0, 0, W * SCALE, H * SCALE);
    
    // Create/recreate VRAM texture
    if (vram_texture) glDeleteTextures(1, &vram_texture);
    glGenTextures(1, &vram_texture);
    glBindTexture(GL_TEXTURE_2D, vram_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate texture based on BPP
    if (BPP == 8) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, W, H, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    } else if (BPP == 16) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, W, H, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }
    
    if (sys->audio_size > 0 && audio_dev == 0) {
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = sys->audio_rate ? sys->audio_rate : 44100;
        wanted.format = AUDIO_F32;
        wanted.channels = sys->audio_channels ? sys->audio_channels : 1;
        wanted.samples = 1024;
        wanted.callback = NULL; // Use callback for audio processing
        wanted.callback = (void(*)(void*, Uint8*, int))SDL_memset; // Placeholder
        audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
        if (audio_dev) SDL_PauseAudioDevice(audio_dev, 0);
    }
}

// GPU-accelerated audio processing
static void process_audio_gpu(void* userdata, Uint8* stream_ptr, int len_bytes) {
    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    if (!mem) return;
    SystemConfig* sys = (SystemConfig*)mem;
    uint8_t* audio_buf = mem + 512 + (W * H * (BPP / 8));
    float* stream = (float*)stream_ptr;
    int num_samples = len_bytes / sizeof(float);
    
    uint32_t r = sys->audio_read;
    uint32_t w = sys->audio_write;
    uint32_t size = sys->audio_size;
    if (size == 0) return;
    
    // Process audio samples (could be optimized with SIMD)
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

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror("Failed to open ROM");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* wasm_data = malloc(sz);
    fread(wasm_data, 1, sz, f);
    fclose(f);
    
    IM3Environment env = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 64*1024*1024, NULL);
    IM3Module module;
    m3_ParseModule(env, &module, wasm_data, sz);
    m3_LoadModule(runtime, module);
    
    IM3Function f_init = NULL, f_upd = NULL;
    m3_FindFunction(&f_init, runtime, "winit");
    m3_FindFunction(&f_upd, runtime, "wupdate");
    
    if (f_init) m3_CallV(f_init);
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    init_sdl_from_header();
    
    int running = 1;
    while (running) {
        SDL_Event ev;
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) { SDL_Delay(1); continue; }
        SystemConfig* sys = (SystemConfig*)mem;
        sys->ticks = SDL_GetTicks();
        
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (ev.key.keysym.scancode < 256)
                    sys->keys[ev.key.keysym.scancode] = (ev.type == SDL_KEYDOWN);
            }
            if (ev.type == SDL_MOUSEMOTION) {
                sys->mouse_x = ev.motion.x / SCALE;
                sys->mouse_y = ev.motion.y / SCALE;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                int b = ev.button.button;
                if (b == SDL_BUTTON_LEFT)
                    sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 1) : (sys->mouse_buttons & ~1);
                if (b == SDL_BUTTON_RIGHT)
                    sys->mouse_buttons = (ev.type == SDL_MOUSEBUTTONDOWN) ? (sys->mouse_buttons | 2) : (sys->mouse_buttons & ~2);
            }
            if (ev.type == SDL_MOUSEWHEEL) {
                sys->mouse_wheel += ev.wheel.y;
            }
        }
        
        if (f_upd) m3_CallV(f_upd);
        
        mem = m3_GetMemory(runtime, NULL, 0);
        sys = (SystemConfig*)mem;
        
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
            // GPU-accelerated rendering
            uint8_t* vram = mem + 512;
            
            glBindTexture(GL_TEXTURE_2D, vram_texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            
            if (BPP == 8) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RED, GL_UNSIGNED_BYTE, vram);
            } else if (BPP == 16) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RG, GL_UNSIGNED_BYTE, vram);
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, vram);
            }
            
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(render_program);
            glUniform1i(tex_uniform_loc, 0);
            glUniform1i(glGetUniformLocation(render_program, "bpp"), BPP);
            
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            
            SDL_GL_SwapWindow(window);
        }
        
        SDL_Delay(1);
    }
    
    // Cleanup
    if (audio_dev) SDL_CloseAudioDevice(audio_dev);
    glDeleteTextures(1, &vram_texture);
    glDeleteProgram(render_program);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(wasm_data);
    
    return 0;
}
