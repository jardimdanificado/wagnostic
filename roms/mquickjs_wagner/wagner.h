#ifndef WAGNER_H
#define WAGNER_H

/**
 * WagnO - Easy Game Development API for Wagnostic
 * 
 * Inspired by p5.js and LÖVE2D, this API provides a simple way to create
 * games and interactive applications for the Wagnostic WASM runtime.
 * 
 * Usage:
 *   #define WAGNER_IMPLEMENTATION
 *   #include "wagner.h"
 * 
 *   void draw() { ... }
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

// ============================================
// WAGNOSTIC RUNTIME LOW-LEVEL INTERFACE
// ============================================

#ifndef WAGNOSTIC_H
#define WAGNOSTIC_H

#ifndef WAGNOSTIC_RECT_DEFINED
#define WAGNOSTIC_RECT_DEFINED
typedef struct {
    int x, y, w, h;
} Rect;
#endif

// GAMEPAD BUTTON CONSTANTS
#define W_BTN_UP     (1 << 0)
#define W_BTN_DOWN   (1 << 1)
#define W_BTN_LEFT   (1 << 2)
#define W_BTN_RIGHT  (1 << 3)
#define W_BTN_A      (1 << 4)
#define W_BTN_B      (1 << 5)
#define W_BTN_X      (1 << 6)
#define W_BTN_Y      (1 << 7)
#define W_BTN_L1     (1 << 8)
#define W_BTN_R1     (1 << 9)
#define W_BTN_START  (1 << 10)
#define W_BTN_SELECT (1 << 11)
#define W_BTN_L2     (1 << 12)
#define W_BTN_R2     (1 << 13)

// COLOR CONVERSION MACROS
#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#define W_RGB332(r, g, b) (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

#define W_MAX_DIRTY_RECTS 32

extern void* wextension(const char* name, void* ptr);

typedef struct {
    int32_t x, y;
    uint32_t buttons;
    int32_t wheel;
} WagnosticMouseState;

typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
} WagnosticState;

typedef struct {
    uint32_t count;
    Rect rects[W_MAX_DIRTY_RECTS];
} WagnosticDirtyList;

typedef struct {
    uint16_t x, y;            // Scissor position (VRAM region)
    uint16_t width, height;   // Scissor size (VRAM region)
    uint32_t shader_ptr;      // Offset to null-terminated GLSL string
    uint32_t params_ptr;      // Offset to float array 'u_params'
} WagnosticShaderJob;

#define SET_BPP(s, bpp_val) do { \
    if ((bpp_val) == 32) { \
        (s)->r_bits=8;(s)->r_shift=0; \
        (s)->g_bits=8;(s)->g_shift=8; \
        (s)->b_bits=8;(s)->b_shift=16; \
        (s)->a_bits=8;(s)->a_shift=24; \
    } else if ((bpp_val) == 16) { \
        (s)->r_bits=5;(s)->r_shift=11; \
        (s)->g_bits=6;(s)->g_shift=5; \
        (s)->b_bits=5;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
    } else if ((bpp_val) == 8) { \
        (s)->r_bits=3;(s)->r_shift=5; \
        (s)->g_bits=3;(s)->g_shift=2; \
        (s)->b_bits=2;(s)->b_shift=0; \
        (s)->a_bits=0;(s)->a_shift=0; \
    } \
} while(0)

#define W_VRAM(s)       ((uint8_t*)(s) + (s)->vram_offset)

static inline void w_setup(WagnosticState *s, const char* title, int width, int height, int bpp, int scale) {
    (void)title; (void)scale;
    s->width = (uint32_t)width;
    s->height = (uint32_t)height;
    SET_BPP(s, bpp);
}

static inline void w_redraw(WagnosticState *s, WagnosticDirtyList *dl) {
    (void)dl;
    Rect r = { 0, 0, (int)s->width, (int)s->height };
    wextension("std:dirty", &r);
}

static inline void w_no_redraw(WagnosticState *s, WagnosticDirtyList *dl) {
    (void)s; (void)dl;
}

static inline void w_redraw_rect(WagnosticState *s, WagnosticDirtyList *dl, int x, int y, int w, int h) {
    (void)s; (void)dl;
    Rect r = { x, y, w, h };
    wextension("std:dirty", &r);
}

#define W_KEY_DOWN(s, scancode) ((s)->keys[scancode] != 0)
#define W_MOUSE_LEFT(s) (((s)->mouse_buttons & 1) != 0)
#define W_MOUSE_RIGHT(s) (((s)->mouse_buttons & 2) != 0)

#endif // WAGNOSTIC_H

// Default Configuration (RGBA8888, 320x240)
#ifndef WAGNER_CFG_W
#define WAGNER_CFG_W 320
#endif
#ifndef WAGNER_CFG_H
#define WAGNER_CFG_H 240
#endif
#ifndef WAGNER_CFG_BPP
#define WAGNER_CFG_BPP 32
#endif
#ifndef WAGNER_CFG_SCALE
#define WAGNER_CFG_SCALE 2
#endif

// Default Color Precision for 32-bit (RGBA8888) if nothing is defined
#ifndef WAGNER_CFG_R_BITS
    #if WAGNER_CFG_BPP == 32
        #define WAGNER_CFG_R_BITS 8
        #define WAGNER_CFG_R_SHIFT 0
        #define WAGNER_CFG_G_BITS 8
        #define WAGNER_CFG_G_SHIFT 8
        #define WAGNER_CFG_B_BITS 8
        #define WAGNER_CFG_B_SHIFT 16
        #define WAGNER_CFG_A_BITS 8
        #define WAGNER_CFG_A_SHIFT 24
    #elif WAGNER_CFG_BPP == 16
        #define WAGNER_CFG_R_BITS 5
        #define WAGNER_CFG_R_SHIFT 11
        #define WAGNER_CFG_G_BITS 6
        #define WAGNER_CFG_G_SHIFT 5
        #define WAGNER_CFG_B_BITS 5
        #define WAGNER_CFG_B_SHIFT 0
        #define WAGNER_CFG_A_BITS 0
        #define WAGNER_CFG_A_SHIFT 0
    #elif WAGNER_CFG_BPP == 8
        #define WAGNER_CFG_R_BITS 3
        #define WAGNER_CFG_R_SHIFT 5
        #define WAGNER_CFG_G_BITS 3
        #define WAGNER_CFG_G_SHIFT 2
        #define WAGNER_CFG_B_BITS 2
        #define WAGNER_CFG_B_SHIFT 0
        #define WAGNER_CFG_A_BITS 0
        #define WAGNER_CFG_A_SHIFT 0
    #else
        #define WAGNER_CFG_R_BITS 0
        #define WAGNER_CFG_R_SHIFT 0
        #define WAGNER_CFG_G_BITS 0
        #define WAGNER_CFG_G_SHIFT 0
        #define WAGNER_CFG_B_BITS 0
        #define WAGNER_CFG_B_SHIFT 0
        #define WAGNER_CFG_A_BITS WAGNER_CFG_BPP
        #define WAGNER_CFG_A_SHIFT 0
    #endif
#endif

// Wagnostic new API: ROM must provide a WagnosticState struct
#define WAGNER_VRAM_SIZE (WAGNER_CFG_BPP >= 8 ? (WAGNER_CFG_W * WAGNER_CFG_H * (WAGNER_CFG_BPP == 24 ? 3 : (WAGNER_CFG_BPP / 8))) : ((WAGNER_CFG_W * WAGNER_CFG_H * WAGNER_CFG_BPP + 7) / 8))

static uint8_t* _wagner_keys_ptr = NULL;
static WagnosticMouseState* _wagner_mouse_ptr = NULL;
static uint32_t* _wagner_gamepad_ptr = NULL;
static uint32_t _wagner_frame_counter = 0;

static struct {
    WagnosticState state;
    WagnosticDirtyList dirty_list;
    uint8_t vram[WAGNER_VRAM_SIZE];
} _wagner_rom;

#define w_width _wagner_rom.state.width
#define w_height _wagner_rom.state.height
#define w_bpp WAGNER_CFG_BPP
#define w_scale 1
#define w_mouse_x (_wagner_mouse_ptr ? _wagner_mouse_ptr->x : 0)
#define w_mouse_y (_wagner_mouse_ptr ? _wagner_mouse_ptr->y : 0)
#define w_mouse_buttons (_wagner_mouse_ptr ? _wagner_mouse_ptr->buttons : 0)
#define w_keys _wagner_keys_ptr
#define w_ticks _wagner_frame_counter
#define w_target_fps 60
#define w_gamepad_buttons (_wagner_gamepad_ptr ? *_wagner_gamepad_ptr : 0)
#define w_unique 12345
#define w_vram _wagner_rom.vram

// No assets.h anymore. Using Wagnostic .tar TAR API for dynamic loading.

// pixel_t is always uint32_t. Runtime BPP (8/16/32) is handled by
// Canvas.bpp and olivec_set_pixel — no compile-time choice needed.
typedef uint64_t pixel_t;

// ============================================
// COLOR CONVERSION (input → pixel_t)
// ============================================

static inline pixel_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint64_t px = 0;
    if (WAGNER_CFG_R_BITS) px |= (((uint64_t)r >> (8 - WAGNER_CFG_R_BITS)) << WAGNER_CFG_R_SHIFT);
    if (WAGNER_CFG_G_BITS) px |= (((uint64_t)g >> (8 - WAGNER_CFG_G_BITS)) << WAGNER_CFG_G_SHIFT);
    if (WAGNER_CFG_B_BITS) px |= (((uint64_t)b >> (8 - WAGNER_CFG_B_BITS)) << WAGNER_CFG_B_SHIFT);
    if (WAGNER_CFG_A_BITS) px |= (((uint64_t)255 >> (8 - WAGNER_CFG_A_BITS)) << WAGNER_CFG_A_SHIFT);
    else if (WAGNER_CFG_BPP >= 24) px |= ((uint64_t)255 << 24);
    if (!WAGNER_CFG_R_BITS && !WAGNER_CFG_G_BITS && !WAGNER_CFG_B_BITS && WAGNER_CFG_A_BITS) {
        uint8_t lum = (uint8_t)(((uint32_t)r * 299 + (uint32_t)g * 587 + (uint32_t)b * 114) / 1000);
        px |= (((uint64_t)lum >> (8 - WAGNER_CFG_A_BITS)) << WAGNER_CFG_A_SHIFT);
    }
    return (pixel_t)px;
}

static inline pixel_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint64_t px = 0;
    if (WAGNER_CFG_R_BITS) px |= (((uint64_t)r >> (8 - WAGNER_CFG_R_BITS)) << WAGNER_CFG_R_SHIFT);
    if (WAGNER_CFG_G_BITS) px |= (((uint64_t)g >> (8 - WAGNER_CFG_G_BITS)) << WAGNER_CFG_G_SHIFT);
    if (WAGNER_CFG_B_BITS) px |= (((uint64_t)b >> (8 - WAGNER_CFG_B_BITS)) << WAGNER_CFG_B_SHIFT);
    if (WAGNER_CFG_A_BITS) px |= (((uint64_t)a >> (8 - WAGNER_CFG_A_BITS)) << WAGNER_CFG_A_SHIFT);
    else if (WAGNER_CFG_BPP >= 24) px |= ((uint64_t)a << 24);
    if (!WAGNER_CFG_R_BITS && !WAGNER_CFG_G_BITS && !WAGNER_CFG_B_BITS && WAGNER_CFG_A_BITS) {
        uint8_t lum = (uint8_t)(((uint32_t)r * 299 + (uint32_t)g * 587 + (uint32_t)b * 114) / 1000);
        px |= (((uint64_t)lum >> (8 - WAGNER_CFG_A_BITS)) << WAGNER_CFG_A_SHIFT);
    }
    return (pixel_t)px;
}

// Predefined colors — always pixel_t
#define BLACK   rgb(0,0,0)
#define WHITE   rgb(255,255,255)
#define RED     rgb(255,0,0)
#define GREEN   rgb(0,255,0)
#define BLUE    rgb(0,0,255)
#define YELLOW  rgb(255,255,0)
#define CYAN    rgb(0,255,255)
#define MAGENTA rgb(255,0,255)
#define GRAY    rgb(128,128,128)
#define ORANGE  rgb(255,165,0)
#define PURPLE  rgb(128,0,128)

// Config defaults — overridden by `wagner build` via -D flags
#ifndef WAGNER_CFG_W
#define WAGNER_CFG_W 320
#define WAGNER_CFG_H 240
#define WAGNER_CFG_BPP 16
#define WAGNER_CFG_SCALE 2
#define WAGNER_TITLE "WagnO"
#endif

// ============================================
// CORE TYPES
// ============================================

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    uint8_t r, g, b, a;
} Color;

// Rect is defined in wagnostic.h

typedef struct { void* pixels; int width; int height; int stride; uint8_t bpp; uint8_t r_bits, r_shift, g_bits, g_shift, b_bits, b_shift, a_bits, a_shift; } Canvas;
typedef Canvas Image;

typedef struct {
    Image *frames;
    int *delays;
    int frame_count;
    int width;
    int height;
} Gif;

// ============================================
// GLOBAL STATE (managed by WagnO)
// ============================================

static struct {
    // Screen config
    int width;
    int height;
    int bpp;
    int scale;
    
    // Time
    float delta_time;
    uint32_t frame_count;
    uint32_t fps;
    
    // Input - Mouse
    Vec2 mouse;
    Vec2 pmouse;  // previous mouse
    bool mouse_pressed;
    bool mouse_released;
    bool mouse_down;
    int mouse_button;  // which button
    
    // Input - Keyboard
    bool keys[256];
    bool keys_pressed[256];
    bool keys_released[256];
    
    // Main canvas (wraps w_vram)
    void* canvas_pixels;
} wagner;

// The default screen canvas — initialized in wupdate()
Canvas screen;

// ============================================
// KEY CODES (SDL / USB HID Scancodes)
// ============================================
#define KEY_A           4
#define KEY_B           5
#define KEY_C           6
#define KEY_D           7
#define KEY_E           8
#define KEY_F           9
#define KEY_G           10
#define KEY_H           11
#define KEY_I           12
#define KEY_J           13
#define KEY_K           14
#define KEY_L           15
#define KEY_M           16
#define KEY_N           17
#define KEY_O           18
#define KEY_P           19
#define KEY_Q           20
#define KEY_R           21
#define KEY_S           22
#define KEY_T           23
#define KEY_U           24
#define KEY_V           25
#define KEY_W           26
#define KEY_X           27
#define KEY_Y           28
#define KEY_Z           29

#define KEY_1           30
#define KEY_2           31
#define KEY_3           32
#define KEY_4           33
#define KEY_5           34
#define KEY_6           35
#define KEY_7           36
#define KEY_8           37
#define KEY_9           38
#define KEY_0           39

#define KEY_ENTER       40
#define KEY_ESCAPE      41
#define KEY_BACKSPACE   42
#define KEY_TAB         43
#define KEY_SPACE       44

#define KEY_RIGHT       79
#define KEY_LEFT        80
#define KEY_DOWN        81
#define KEY_UP          82

#define KEY_MINUS       45
#define KEY_EQUALS      46
#define KEY_PLUS        46
#define KEY_KP_MINUS    86
#define KEY_KP_PLUS     87

// ============================================
// MATH CONSTANTS
// ============================================


#define PI 3.14159265358979f
#define TWO_PI 6.28318530717959f
#define HALF_PI 1.5707963267949f

static inline float map(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

static inline float constrain(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}


static inline float w_sqrt(float x) {
    // Newton's method approximation
    if (x <= 0) return 0;
    float guess = x / 2.0f;
    for (int i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

static inline float dist_sq(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;  // squared distance — fast, no sqrt
}

static inline float dist(float x1, float y1, float x2, float y2) {
    return w_sqrt(dist_sq(x1, y1, x2, y2));
}


static inline float wabs(float x) {
    return x < 0 ? -x : x;
}

static inline float min(float a, float b) {
    return a < b ? a : b;
}

static inline float max(float a, float b) {
    return a > b ? a : b;
}

// Trigonometric functions using Bhaskara I approximation
static inline float w_sin(float x) {
    // Normalize to [0, 2π] using division to avoid O(N) loop
    if (x < 0.0f || x >= TWO_PI) {
        int q = (int)(x / TWO_PI);
        x = x - (q * TWO_PI);
        if (x < 0.0f) x += TWO_PI;
    }
    
    if (x > PI) {
        float y = x - PI;
        return -16.0f * y * (PI - y) / (5.0f * PI * PI - 4.0f * y * (PI - y));
    }
    return 16.0f * x * (PI - x) / (5.0f * PI * PI - 4.0f * x * (PI - x));
}

static inline float w_cos(float x) {
    return w_sin(x + HALF_PI);
}

static uint32_t _wagner_rng_seed = 12345;

static inline void random_seed(uint32_t seed) {
    _wagner_rng_seed = seed ? seed : 1;
}

static inline int random_int(int min_val, int max_val) {
    _wagner_rng_seed = _wagner_rng_seed * 1103515245 + 12345;
    return min_val + (int)((_wagner_rng_seed >> 16) % (uint32_t)(max_val - min_val + 1));
}

static inline float wagner_random(float min_val, float max_val) {
    return min_val + (float)random_int(0, 10000) / 10000.0f * (max_val - min_val);
}

// ============================================
// COLOR HELPERS (return pixel_t, BPP-aware)
// ============================================

static inline pixel_t hex(uint32_t h) {
    return rgb((h>>16)&0xFF, (h>>8)&0xFF, h&0xFF);
}

static inline pixel_t hexa(uint32_t h) {
    return rgba((h>>24)&0xFF, (h>>16)&0xFF, (h>>8)&0xFF, h&0xFF);
}

// ============================================
// VECTOR FUNCTIONS
// ============================================

static inline Vec2 vec2(float x, float y) {
    return (Vec2){x, y};
}

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

static inline Vec2 vec2_mul(Vec2 v, float s) {
    return (Vec2){v.x * s, v.y * s};
}

static inline float vec2_len(Vec2 v) {
    return w_sqrt(v.x * v.x + v.y * v.y);
}

static inline Vec2 vec2_normalize(Vec2 v) {
    float len = vec2_len(v);
    if (len == 0) return (Vec2){0, 0};
    return (Vec2){v.x / len, v.y / len};
}

// ============================================
// CANVAS FUNCTIONS
// ============================================

Canvas canvas_sub(Canvas src, int x, int y, int w, int h);

// ============================================
// TRANSFORMS & STATE
// ============================================

typedef struct {
    float a, c, e;
    float b, d, f;
} WagnerMatrix;
typedef WagnerMatrix Matrix;

typedef struct {
    WagnerMatrix matrix;
    Canvas target;
    bool has_target;
    pixel_t fill_color;
    pixel_t stroke_color;
    bool has_fill;
    bool has_stroke;
    Canvas texture;
    bool has_texture;
    bool has_color_key;
    uint32_t color_key;
} WagnerRenderState;
typedef WagnerRenderState RenderState;

void push(void);
void pop(void);
void translate(float x, float y);
void rotate(float angle);
void scale(float sx, float sy);
void apply_matrix(float a, float b, float c, float d, float e, float f);

void render_target(Canvas c);
void reset_target(void);
void fill(pixel_t color);
void no_fill(void);
void stroke(pixel_t color);
void no_stroke(void);
void texture(Canvas img);
void no_texture(void);
void color_key(uint32_t key);
void no_color_key(void);

// ============================================
// DRAWING PRIMITIVES
// ============================================

void clear(void);
void rect(void);
void circle(void);
void triangle(void); // Unit triangle
void triangle_pts(float x1, float y1, float x2, float y2, float x3, float y3);
void line(float x1, float y1, float x2, float y2);
void pixel(float x, float y);

// Pixel access
pixel_t pixel_at(Canvas c, int x, int y);

// Texture-mapped triangle (perspective-correct UV)
void triangle3uv(int x1, int y1, int x2, int y2, int x3, int y3,
    float tx1, float ty1, float tx2, float ty2, float tx3, float ty3,
    float z1, float z2, float z3, Canvas texture);

// ============================================
// TEXT FUNCTIONS
// ============================================

void text(const char* text);
int text_width(const char* text);

// ============================================
// IMAGE & RESOURCE FUNCTIONS
// ============================================

Canvas canvas_create(int w, int h, int bpp);
Image  create_image(int width, int height);
Canvas img_create(const void* data, int width, int height, int bpp);
Canvas img_load(const uint8_t* data, size_t size);
Image  load_image(const char* path);
static inline pixel_t lerp_color(pixel_t a, pixel_t b, float t);
static inline int text_height(void);

typedef struct {
    void* data;
    uint32_t size;
} WagnerData;
typedef WagnerData Data;

Data  load_data(const char* path);

void set_fps(uint32_t fps);
void set_fps(uint32_t fps) { (void)fps; }

// ============================================
// USER FUNCTIONS (implemented by user)
// ============================================

// Defaults for config — overridden by `wagner build` via -DWAGNER_CFG_* flags

void preload(void);
void setup(void);
void draw(void);

// ============================================
// IMPLEMENTATION
// ============================================

// Weak declarations for optional callbacks. If undefined, their address is NULL.
__attribute__((weak)) void preload(void);
__attribute__((weak)) void setup(void);

// ============================================
// NATIVE RASTERIZATION ENGINE & FONT GLYPHS
// Adapted with credits to Alexey Kutepov (tsoding/olive.c)
// ============================================

static const char _wagner_font_glyphs[128][8][8] = {
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,0,0,0},{0,0,1,1,1,1,0,0},{0,0,1,1,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,0,1,1,0},{0,0,1,1,0,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,0,1,1,0},{0,0,1,1,0,1,1,0},{0,1,1,1,1,1,1,1},{0,0,1,1,0,1,1,0},{0,1,1,1,1,1,1,1},{0,0,1,1,0,1,1,0},{0,0,1,1,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,1,0,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,1,1},{0,0,0,1,1,1,1,0},{0,0,1,1,0,0,0,0},{0,1,1,1,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,1,1,0,0,0,1,1},{0,0,1,1,0,0,1,1},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,1,1,0,1,1,1,0},{0,0,1,1,1,0,1,1},{0,0,1,1,0,0,1,1},{0,1,1,0,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,0,0},{1,1,1,1,1,1,1,1},{0,0,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,1,1,1},{0,1,1,0,1,1,1,1},{0,1,1,1,1,0,1,1},{0,1,1,1,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,1,0,0},{0,0,0,1,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,0,0,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,1,0},{0,0,0,1,1,1,1,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,1,1},{0,0,0,0,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,1},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,1,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,1,1,1,1},{0,1,1,0,1,0,1,1},{0,1,1,0,1,1,1,1},{0,1,1,0,0,0,0,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,1,1,1,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,1,0},{0,0,1,1,0,0,1,1},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,1,1,0,0,1,1},{0,0,0,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,0,0,0},{0,1,1,0,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,1,1,0,0},{0,1,1,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,1,0},{0,0,1,1,0,0,1,1},{0,1,1,0,0,0,0,0},{0,1,1,0,1,1,1,0},{0,1,1,0,0,0,1,1},{0,0,1,1,0,0,1,1},{0,0,0,1,1,1,0,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,1,1,1,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,1,1,0,1,1,0,0},{0,0,1,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,1,1,0},{0,1,1,0,1,1,0,0},{0,1,1,1,1,0,0,0},{0,1,1,1,0,0,0,0},{0,1,1,1,1,0,0,0},{0,1,1,0,1,1,0,0},{0,1,1,0,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,1,0,1,1,1},{0,1,1,1,1,1,1,1},{0,1,1,0,1,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,1,1,1},{0,1,1,0,1,1,1,1},{0,1,1,1,1,0,1,1},{0,1,1,1,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,0,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,1,1,1,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,1,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,0,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,1,1,1,0,0},{0,1,1,0,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,0,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,0,0,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,1,0,1,1},{0,1,1,1,1,1,1,1},{0,1,1,1,0,1,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,1,1,1,1},{0,0,0,0,0,0,1,1},{0,0,0,0,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,1,1,0},{0,0,0,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,1,1,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,0,0,0},{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1}},
    {{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,1,0},{0,1,1,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,1,1,1,1,1,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,1,1,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,1,1,0},{0,0,1,1,1,1,0,0}},
    {{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,1,1,0,0},{0,1,1,1,1,0,0,0},{0,1,1,0,1,1,0,0},{0,1,1,0,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,1,1},{0,1,1,1,1,1,1,1},{0,1,1,0,1,0,1,1},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,1,1,0},{0,0,0,0,0,1,1,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,1,1,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,1,1,1,1,1,0},{0,1,1,0,0,0,0,0},{0,0,1,1,1,1,0,0},{0,0,0,0,0,1,1,0},{0,1,1,1,1,1,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,0,0,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,0,1,1},{0,1,1,0,0,0,1,1},{0,1,1,0,1,0,1,1},{0,1,1,1,1,1,1,1},{0,0,1,1,0,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,0,1,1},{0,0,1,1,0,1,1,0},{0,0,0,1,1,1,0,0},{0,0,1,1,0,1,1,0},{0,1,1,0,0,0,1,1},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,1,1,0,0,1,1,0},{0,0,1,1,1,1,1,0},{0,0,0,0,0,1,1,0},{0,0,1,1,1,1,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,1,1,0,0},{0,0,0,1,1,0,0,0},{0,0,1,1,0,0,0,0},{0,1,1,1,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,1,1,1,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,1,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,1,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,1,1,1,0,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,0,0,0,1,1,1,0},{0,0,0,1,1,0,0,0},{0,0,0,1,1,0,0,0},{0,1,1,1,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,1,1,1,0,1,1},{0,1,1,0,1,1,1,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
    {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}},
};

static inline void _wagner_set_pixel_raw(Canvas c, int x, int y, pixel_t color) {
    if (x < 0 || x >= c.width || y < 0 || y >= c.height) return;
    size_t idx = (size_t)y * c.stride + x;
    if (c.bpp == 32) ((uint32_t*)c.pixels)[idx] = (uint32_t)color;
    else if (c.bpp == 24) {
        uint8_t *p = &((uint8_t*)c.pixels)[idx * 3];
        p[0] = (uint8_t)color; p[1] = (uint8_t)(color >> 8); p[2] = (uint8_t)(color >> 16);
    }
    else if (c.bpp == 16) ((uint16_t*)c.pixels)[idx] = (uint16_t)color;
    else if (c.bpp == 8)  ((uint8_t*)c.pixels)[idx] = (uint8_t)color;
}

static inline pixel_t _wagner_get_pixel_raw(Canvas c, int x, int y) {
    if (x < 0 || x >= c.width || y < 0 || y >= c.height) return 0;
    size_t idx = (size_t)y * c.stride + x;
    if (c.bpp == 32) return ((uint32_t*)c.pixels)[idx];
    if (c.bpp == 24) {
        uint8_t *p = &((uint8_t*)c.pixels)[idx * 3];
        return (pixel_t)(p[0] | (p[1] << 8) | (p[2] << 16));
    }
    if (c.bpp == 16) return ((uint16_t*)c.pixels)[idx];
    if (c.bpp == 8) return ((uint8_t*)c.pixels)[idx];
    return 0;
}

static inline void _wagner_fill(Canvas c, pixel_t color) {
    size_t n = (size_t)c.width * c.height;
    if (c.bpp == 32) { uint32_t *p = (uint32_t*)c.pixels; uint32_t col = (uint32_t)color; while(n--) *p++ = col; }
    else if (c.bpp == 24) {
        uint8_t *p = (uint8_t*)c.pixels;
        uint8_t r = (uint8_t)color, g = (uint8_t)(color >> 8), b = (uint8_t)(color >> 16);
        while(n--) { *p++ = r; *p++ = g; *p++ = b; }
    }
    else if (c.bpp == 16) { uint16_t *p = (uint16_t*)c.pixels; uint16_t col = (uint16_t)color; while(n--) *p++ = col; }
    else if (c.bpp == 8)  { uint8_t *p = (uint8_t*)c.pixels; uint8_t col = (uint8_t)color; while(n--) *p++ = col; }
}

static inline void _wagner_rect(Canvas c, int x, int y, int w, int h, pixel_t color) {
    if (w == 0 || h == 0) return;
    int x1 = x, x2 = x + (w > 0 ? w - 1 : w + 1);
    int y1 = y, y2 = y + (h > 0 ? h - 1 : h + 1);
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x1 >= c.width || x2 < 0 || y1 >= c.height || y2 < 0) return;
    if (x1 < 0) x1 = 0; if (y1 < 0) y1 = 0;
    if (x2 >= c.width) x2 = c.width - 1;
    if (y2 >= c.height) y2 = c.height - 1;
    for (int iy = y1; iy <= y2; ++iy) {
        for (int ix = x1; ix <= x2; ++ix) _wagner_set_pixel_raw(c, ix, iy, color);
    }
}

static inline void _wagner_line(Canvas c, int x1, int y1, int x2, int y2, pixel_t color) {
    int dx = (x2 - x1 >= 0 ? x2 - x1 : x1 - x2);
    int dy = -(y2 - y1 >= 0 ? y2 - y1 : y1 - y2);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        _wagner_set_pixel_raw(c, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

static inline void _wagner_circle(Canvas c, int cx, int cy, int r, pixel_t color) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        _wagner_set_pixel_raw(c, cx+x, cy+y, color); _wagner_set_pixel_raw(c, cx+y, cy+x, color);
        _wagner_set_pixel_raw(c, cx-y, cy+x, color); _wagner_set_pixel_raw(c, cx-x, cy+y, color);
        _wagner_set_pixel_raw(c, cx-x, cy-y, color); _wagner_set_pixel_raw(c, cx-y, cy-x, color);
        _wagner_set_pixel_raw(c, cx+y, cy-x, color); _wagner_set_pixel_raw(c, cx+x, cy-y, color);
        if (err <= 0) { y += 1; err += 2*y + 1; }
        if (err > 0) { x -= 1; err -= 2*x + 1; }
    }
}

static inline bool _wagner_barycentric(int x1, int y1, int x2, int y2, int x3, int y3, int xp, int yp, int *u1, int *u2, int *det) {
    *det = ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
    *u1  = ((y2 - y3)*(xp - x3) + (x3 - x2)*(yp - y3));
    *u2  = ((y3 - y1)*(xp - x3) + (x1 - x3)*(yp - y3));
    int u3 = *det - *u1 - *u2;
    int s1 = (*u1 > 0) - (*u1 < 0), s2 = (*u2 > 0) - (*u2 < 0), s3 = (u3 > 0) - (u3 < 0), sd = (*det > 0) - (*det < 0);
    return ((s1 == sd || *u1 == 0) && (s2 == sd || *u2 == 0) && (s3 == sd || u3 == 0));
}

static inline void _wagner_triangle(Canvas c, int x1, int y1, int x2, int y2, int x3, int y3, pixel_t color) {
    int lx = x1, hx = x1, ly = y1, hy = y1;
    if (x2 < lx) lx = x2; if (x3 < lx) lx = x3; if (x2 > hx) hx = x2; if (x3 > hx) hx = x3;
    if (y2 < ly) ly = y2; if (y3 < ly) ly = y3; if (y2 > hy) hy = y2; if (y3 > hy) hy = y3;
    if (lx < 0) lx = 0; if (hx >= c.width) hx = c.width - 1;
    if (ly < 0) ly = 0; if (hy >= c.height) hy = c.height - 1;
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            int u1, u2, det;
            if (_wagner_barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &det)) {
                _wagner_set_pixel_raw(c, x, y, color);
            }
        }
    }
}

static inline void _wagner_text(Canvas c, const char *text, int x, int y, int size, pixel_t color) {
    for (size_t i = 0; text[i] != '\0'; ++i) {
        int gx = x + (int)(i * 8 * size);
        unsigned char ch = (unsigned char)text[i];
        if (ch >= 128) ch = '?';
        for (size_t j = 0; j < 8; ++j) {
            for (size_t k = 0; k < 8; ++k) {
                if (_wagner_font_glyphs[ch][j][k]) {
                    _wagner_rect(c, gx + (int)(k * size), y + (int)(j * size), size, size, color);
                }
            }
        }
    }
}

// ============================================
// CANVAS & DRAWING IMPLEMENTATION
// ============================================

Canvas canvas_sub(Canvas src, int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > src.width) w = src.width - x;
    if (y + h > src.height) h = src.height - y;
    if (w <= 0 || h <= 0) return (Canvas){0};
    size_t bpp_bytes = (src.bpp == 24) ? 3 : (src.bpp / 8);
    void* pixels = (uint8_t*)src.pixels + (y * src.stride + x) * bpp_bytes;
    Canvas c = src;
    c.pixels = pixels;
    c.width = w;
    c.height = h;
    return c;
}

static WagnerRenderState _wagner_state_stack[32] = {
    { {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}, {0}, false, 0, 0, false, false, {0}, false, false, 0 }
};
static int _wagner_state_sp = 0;

static inline WagnerRenderState* _wagner_current_state(void) {
    return &_wagner_state_stack[_wagner_state_sp];
}

static inline WagnerMatrix* _wagner_current_matrix(void) {
    return &_wagner_state_stack[_wagner_state_sp].matrix;
}

void push(void) {
    if (_wagner_state_sp < 31) {
        _wagner_state_stack[_wagner_state_sp + 1] = _wagner_state_stack[_wagner_state_sp];
        _wagner_state_sp++;
    }
}

void pop(void) {
    if (_wagner_state_sp > 0) {
        _wagner_state_sp--;
    }
}

static inline Canvas _wagner_get_target(void) {
    WagnerRenderState* s = _wagner_current_state();
    return s->has_target ? s->target : screen;
}

void render_target(Canvas c) { WagnerRenderState* s = _wagner_current_state(); s->target = c; s->has_target = true; }
void reset_target(void) { WagnerRenderState* s = _wagner_current_state(); s->has_target = false; }
void fill(pixel_t color) { _wagner_current_state()->has_fill = true; _wagner_current_state()->fill_color = color; }
void no_fill(void) { _wagner_current_state()->has_fill = false; }
void stroke(pixel_t color) { _wagner_current_state()->has_stroke = true; _wagner_current_state()->stroke_color = color; }
void no_stroke(void) { _wagner_current_state()->has_stroke = false; }
void texture(Canvas img) { _wagner_current_state()->texture = img; _wagner_current_state()->has_texture = true; }
void no_texture(void) { _wagner_current_state()->has_texture = false; }
void color_key(uint32_t key) { _wagner_current_state()->has_color_key = true; _wagner_current_state()->color_key = key; }
void no_color_key(void) { _wagner_current_state()->has_color_key = false; }

static inline void _wagner_mat_mul(WagnerMatrix* m, float a, float b, float c, float d, float e, float f) {
    float nm_a = m->a * a + m->c * b;
    float nm_b = m->b * a + m->d * b;
    float nm_c = m->a * c + m->c * d;
    float nm_d = m->b * c + m->d * d;
    float nm_e = m->a * e + m->c * f + m->e;
    float nm_f = m->b * e + m->d * f + m->f;
    m->a = nm_a; m->b = nm_b; m->c = nm_c; m->d = nm_d; m->e = nm_e; m->f = nm_f;
}

void translate(float x, float y) {
    _wagner_mat_mul(_wagner_current_matrix(), 1.0f, 0.0f, 0.0f, 1.0f, x, y);
}

void rotate(float angle) {
    float c = w_cos(angle);
    float s = w_sin(angle);
    _wagner_mat_mul(_wagner_current_matrix(), c, s, -s, c, 0.0f, 0.0f);
}

void scale(float sx, float sy) {
    _wagner_mat_mul(_wagner_current_matrix(), sx, 0.0f, 0.0f, sy, 0.0f, 0.0f);
}

void apply_matrix(float a, float b, float c, float d, float e, float f) {
    _wagner_mat_mul(_wagner_current_matrix(), a, b, c, d, e, f);
}

static inline void _wagner_transform(float* x, float* y) {
    WagnerMatrix* m = _wagner_current_matrix();
    float nx = m->a * (*x) + m->c * (*y) + m->e;
    float ny = m->b * (*x) + m->d * (*y) + m->f;
    *x = nx;
    *y = ny;
}

static inline int _wagner_is_axis_aligned(void) {
    WagnerMatrix* m = _wagner_current_matrix();
    return (wabs(m->b) < 0.001f && wabs(m->c) < 0.001f);
}

static inline Color _pixel_to_rgba(Canvas img, int index) {
    Color c = {0,0,0,255};
    uint64_t px = 0;
    if (img.bpp == 32) px = ((uint32_t*)img.pixels)[index];
    else if (img.bpp == 24) {
        uint8_t* p = &((uint8_t*)img.pixels)[index * 3];
        px = p[0] | (p[1]<<8) | (p[2]<<16);
    }
    else if (img.bpp == 16) px = ((uint16_t*)img.pixels)[index];
    else if (img.bpp == 8) px = ((uint8_t*)img.pixels)[index];
    
    if (!img.r_bits && !img.g_bits && !img.b_bits) {
        if (img.a_bits) {
            uint8_t lum = (uint8_t)(((px >> img.a_shift) & ((1ULL << img.a_bits) - 1)) * 255 / ((1ULL << img.a_bits) - 1));
            c.r = c.g = c.b = lum;
            c.a = 255;
        }
    } else {
        if (img.r_bits) c.r = (uint8_t)(((px >> img.r_shift) & ((1ULL << img.r_bits) - 1)) * 255 / ((1ULL << img.r_bits) - 1));
        if (img.g_bits) c.g = (uint8_t)(((px >> img.g_shift) & ((1ULL << img.g_bits) - 1)) * 255 / ((1ULL << img.g_bits) - 1));
        if (img.b_bits) c.b = (uint8_t)(((px >> img.b_shift) & ((1ULL << img.b_bits) - 1)) * 255 / ((1ULL << img.b_bits) - 1));
        if (img.a_bits) c.a = (uint8_t)(((px >> img.a_shift) & ((1ULL << img.a_bits) - 1)) * 255 / ((1ULL << img.a_bits) - 1));
        else c.a = 255;
    }
    return c;
}

static inline void _canvas_set_pixel(Canvas c, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || x >= c.width || y < 0 || y >= c.height) return;
    uint64_t px = 0;
    if (c.r_bits) px |= (((uint64_t)r >> (8 - c.r_bits)) << c.r_shift);
    if (c.g_bits) px |= (((uint64_t)g >> (8 - c.g_bits)) << c.g_shift);
    if (c.b_bits) px |= (((uint64_t)b >> (8 - c.b_bits)) << c.b_shift);
    if (c.a_bits) px |= (((uint64_t)a >> (8 - c.a_bits)) << c.a_shift);
    if (!c.r_bits && !c.g_bits && !c.b_bits && c.a_bits) {
        uint8_t lum = (uint8_t)(((uint32_t)r * 299 + (uint32_t)g * 587 + (uint32_t)b * 114) / 1000);
        px |= (((uint64_t)lum >> (8 - c.a_bits)) << c.a_shift);
    }
    
    size_t idx = y * c.stride + x;
    if (c.bpp == 64) ((uint64_t*)c.pixels)[idx] = px;
    else if (c.bpp == 32) ((uint32_t*)c.pixels)[idx] = (uint32_t)px;
    else if (c.bpp == 24) {
        uint8_t* p = &((uint8_t*)c.pixels)[idx * 3];
        p[0] = px & 0xFF; p[1] = (px >> 8) & 0xFF; p[2] = (px >> 16) & 0xFF;
    }
    else if (c.bpp == 16) ((uint16_t*)c.pixels)[idx] = (uint16_t)px;
    else if (c.bpp == 8) ((uint8_t*)c.pixels)[idx] = (uint8_t)px;
}

void clear(void) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    if (!state->has_fill) return;
    _wagner_fill(c, state->fill_color);
}

static void _wagner_triangle_textured(Canvas c, int x1, int y1, int x2, int y2, int x3, int y3, 
                                     float tx1, float ty1, float tx2, float ty2, float tx3, float ty3, 
                                     Canvas tex, int has_tint, pixel_t tint_color) {
    int lx = x1, hx = x1, ly = y1, hy = y1;
    if (x2 < lx) lx = x2; if (x3 < lx) lx = x3; if (x2 > hx) hx = x2; if (x3 > hx) hx = x3;
    if (y2 < ly) ly = y2; if (y3 < ly) ly = y3; if (y2 > hy) hy = y2; if (y3 > hy) hy = y3;
    if (lx < 0) lx = 0; if (hx >= (int)c.width) hx = (int)c.width - 1;
    if (ly < 0) ly = 0; if (hy >= (int)c.height) hy = (int)c.height - 1;
    
    // Convert tint color to RGBA if needed
    Color tint = {255, 255, 255, 255};
    if (has_tint) {
        Canvas tmp_c = c;
        tmp_c.pixels = &tint_color;
        tmp_c.width = 1; tmp_c.stride = 1; tmp_c.height = 1;
        tint = _pixel_to_rgba(tmp_c, 0);
    }

    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            int u1, u2, det;
            if (_wagner_barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &det)) {
                float u3 = (float)(det - u1 - u2) / det, fu1 = (float)u1 / det, fu2 = (float)u2 / det;
                float tx = tx1*fu1 + tx2*fu2 + tx3*u3;
                float ty = ty1*fu1 + ty2*fu2 + ty3*u3;
                
                int sx = (int)(tx * tex.width);
                int sy = (int)(ty * tex.height);
                if (sx < 0) sx = 0; if (sx >= tex.width) sx = tex.width - 1;
                if (sy < 0) sy = 0; if (sy >= tex.height) sy = tex.height - 1;
                
                Color col = _pixel_to_rgba(tex, sy * tex.width + sx);
                
                if (has_tint) {
                    col.r = (col.r * tint.r) / 255;
                    col.g = (col.g * tint.g) / 255;
                    col.b = (col.b * tint.b) / 255;
                    col.a = (col.a * tint.a) / 255;
                }
                
                if (col.a > 128) {
                    _canvas_set_pixel(c, x, y, col.r, col.g, col.b, col.a);
                }
            }
        }
    }
}

void rect(void) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    
    if (state->has_texture) {
        Canvas img = state->texture;
        if (_wagner_is_axis_aligned()) {
            float fx = 0.0f, fy = 0.0f; _wagner_transform(&fx, &fy);
            WagnerMatrix* m = _wagner_current_matrix();
            int nx = (int)fx, ny = (int)fy, nw = (int)m->a, nh = (int)m->d;
            if (nw < 0) { nx += nw; nw = -nw; }
            if (nh < 0) { ny += nh; nh = -nh; }
            
            for (int iy = 0; iy < nh; iy++) {
                for (int ix = 0; ix < nw; ix++) {
                    int px = nx + ix, py = ny + iy;
                    if (px < 0 || px >= c.width || py < 0 || py >= c.height) continue;
                    int sx = ix * img.width / nw, sy = iy * img.height / nh;
                    
                    if (state->has_color_key) {
                        uint32_t p = 0;
                        if (img.bpp == 32) p = ((uint32_t*)img.pixels)[sy * img.width + sx];
                        else if (img.bpp == 24) {
                            uint8_t* pxc = &((uint8_t*)img.pixels)[(sy * img.width + sx) * 3];
                            p = pxc[0] | (pxc[1]<<8) | (pxc[2]<<16);
                        } else if (img.bpp == 16) p = ((uint16_t*)img.pixels)[sy * img.width + sx];
                        else p = ((uint8_t*)img.pixels)[sy * img.width + sx];
                        if (p == state->color_key) continue;
                    }
                    
                    Color col = _pixel_to_rgba(img, sy * img.width + sx);
                    if (img.bpp == 32 && col.a <= 128) continue;
                    _canvas_set_pixel(c, px, py, col.r, col.g, col.b, col.a);
                }
            }
        } else {
            float x1 = 0.0f, y1 = 0.0f; _wagner_transform(&x1, &y1);
            float x2 = 1.0f, y2 = 0.0f; _wagner_transform(&x2, &y2);
            float x3 = 1.0f, y3 = 1.0f; _wagner_transform(&x3, &y3);
            float x4 = 0.0f, y4 = 1.0f; _wagner_transform(&x4, &y4);
            _wagner_triangle_textured(c, (int)x1, (int)y1, (int)x2, (int)y2, (int)x4, (int)y4, 0, 0, 1, 0, 0, 1, img, state->has_fill, state->fill_color);
            _wagner_triangle_textured(c, (int)x2, (int)y2, (int)x3, (int)y3, (int)x4, (int)y4, 1, 0, 1, 1, 0, 1, img, state->has_fill, state->fill_color);
        }
    } else if (state->has_fill) {
        if (_wagner_is_axis_aligned()) {
            float fx = 0, fy = 0; _wagner_transform(&fx, &fy);
            WagnerMatrix* m = _wagner_current_matrix();
            int nx = (int)fx, ny = (int)fy, nw = (int)(1.0f * m->a), nh = (int)(1.0f * m->d);
            if (nw < 0) { nx += nw; nw = -nw; }
            if (nh < 0) { ny += nh; nh = -nh; }
            _wagner_rect(c, nx, ny, nw, nh, state->fill_color);
        } else {
            float x1 = 0.0f, y1 = 0.0f; _wagner_transform(&x1, &y1);
            float x2 = 1.0f, y2 = 0.0f; _wagner_transform(&x2, &y2);
            float x3 = 1.0f, y3 = 1.0f; _wagner_transform(&x3, &y3);
            float x4 = 0.0f, y4 = 1.0f; _wagner_transform(&x4, &y4);
            _wagner_triangle(c, (int)x1, (int)y1, (int)x2, (int)y2, (int)x4, (int)y4, state->fill_color);
            _wagner_triangle(c, (int)x2, (int)y2, (int)x3, (int)y3, (int)x4, (int)y4, state->fill_color);
        }
    }
    
    if (state->has_stroke) {
        if (_wagner_is_axis_aligned()) {
            float fx = 0.0f, fy = 0.0f; _wagner_transform(&fx, &fy);
            WagnerMatrix* m = _wagner_current_matrix();
            int nx = (int)fx, ny = (int)fy, nw = (int)m->a, nh = (int)m->d;
            if (nw < 0) { nx += nw; nw = -nw; }
            if (nh < 0) { ny += nh; nh = -nh; }
            int rx = (nw > 0) ? (nx + nw - 1) : nx;
            int by = (nh > 0) ? (ny + nh - 1) : ny;
            _wagner_line(c, nx, ny, rx, ny, state->stroke_color);
            _wagner_line(c, rx, ny, rx, by, state->stroke_color);
            _wagner_line(c, rx, by, nx, by, state->stroke_color);
            _wagner_line(c, nx, by, nx, ny, state->stroke_color);
        } else {
            float x1 = 0.0f, y1 = 0.0f; _wagner_transform(&x1, &y1);
            float x2 = 1.0f, y2 = 0.0f; _wagner_transform(&x2, &y2);
            float x3 = 1.0f, y3 = 1.0f; _wagner_transform(&x3, &y3);
            float x4 = 0.0f, y4 = 1.0f; _wagner_transform(&x4, &y4);
            _wagner_line(c, (int)x1, (int)y1, (int)x2, (int)y2, state->stroke_color);
            _wagner_line(c, (int)x2, (int)y2, (int)x3, (int)y3, state->stroke_color);
            _wagner_line(c, (int)x3, (int)y3, (int)x4, (int)y4, state->stroke_color);
            _wagner_line(c, (int)x4, (int)y4, (int)x1, (int)y1, state->stroke_color);
        }
    }
}

void circle(void) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    
    if (state->has_texture) {
        Canvas img = state->texture;
        float cx_f = 0, cy_f = 0; _wagner_transform(&cx_f, &cy_f);
        float last_x = 1.0f, last_y = 0.0f; _wagner_transform(&last_x, &last_y);
        
        float last_u = 1.0f, last_v = 0.5f;
        for (int i = 1; i <= 32; i++) {
            float ang = i * TWO_PI / 32.0f;
            float px = w_cos(ang), py = w_sin(ang);
            float pu = px * 0.5f + 0.5f;
            float pv = py * 0.5f + 0.5f;
            _wagner_transform(&px, &py);
            
            _wagner_triangle_textured(c, (int)cx_f, (int)cy_f, (int)last_x, (int)last_y, (int)px, (int)py, 0.5f, 0.5f, last_u, last_v, pu, pv, img, state->has_fill, state->fill_color);
            last_x = px; last_y = py;
            last_u = pu; last_v = pv;
        }
    } else if (state->has_fill) {
        if (_wagner_is_axis_aligned()) {
            float fx = 0, fy = 0; _wagner_transform(&fx, &fy);
            WagnerMatrix* m = _wagner_current_matrix();
            float rx = wabs(m->a), ry = wabs(m->d);
            
            if (wabs(rx - ry) < 0.001f) {
                int r = (int)rx;
                for (int y = -r; y <= r; y++) {
                    int dx = (int)w_sqrt((float)(r * r - y * y));
                    for (int x = -dx; x <= dx; x++) _wagner_set_pixel_raw(c, (int)fx + x, (int)fy + y, state->fill_color);
                }
            } else {
                int irx = (int)rx, iry = (int)ry;
                if (irx > 0 && iry > 0) {
                    for (int iy = -iry; iy <= iry; iy++)
                        for (int ix = -irx; ix <= irx; ix++)
                            if ((float)(ix*ix)/(irx*irx) + (float)(iy*iy)/(iry*iry) <= 1.0f)
                                _wagner_set_pixel_raw(c, (int)fx + ix, (int)fy + iy, state->fill_color);
                }
            }
        } else {
            float cx_f = 0, cy_f = 0; _wagner_transform(&cx_f, &cy_f);
            float last_x = 1.0f, last_y = 0.0f; _wagner_transform(&last_x, &last_y);
            for (int i = 1; i <= 32; i++) {
                float ang = i * TWO_PI / 32.0f;
                float px = w_cos(ang), py = w_sin(ang); _wagner_transform(&px, &py);
                _wagner_triangle(c, (int)cx_f, (int)cy_f, (int)last_x, (int)last_y, (int)px, (int)py, state->fill_color);
                last_x = px; last_y = py;
            }
        }
    }
    if (state->has_stroke) {
        if (_wagner_is_axis_aligned()) {
            float fx = 0, fy = 0; _wagner_transform(&fx, &fy);
            WagnerMatrix* m = _wagner_current_matrix();
            _wagner_circle(c, (int)fx, (int)fy, (int)wabs(m->a), state->stroke_color);
        } else {
            float last_x = 1.0f, last_y = 0.0f; _wagner_transform(&last_x, &last_y);
            for (int i = 1; i <= 32; i++) {
                float ang = i * TWO_PI / 32.0f;
                float px = w_cos(ang), py = w_sin(ang); _wagner_transform(&px, &py);
                _wagner_line(c, (int)last_x, (int)last_y, (int)px, (int)py, state->stroke_color);
                last_x = px; last_y = py;
            }
        }
    }
}

void triangle_pts(float x1, float y1, float x2, float y2, float x3, float y3) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    float fx1 = x1, fy1 = y1; _wagner_transform(&fx1, &fy1);
    float fx2 = x2, fy2 = y2; _wagner_transform(&fx2, &fy2);
    float fx3 = x3, fy3 = y3; _wagner_transform(&fx3, &fy3);
    if (state->has_fill) _wagner_triangle(c, (int)fx1, (int)fy1, (int)fx2, (int)fy2, (int)fx3, (int)fy3, state->fill_color);
    if (state->has_stroke) {
        _wagner_line(c, (int)fx1, (int)fy1, (int)fx2, (int)fy2, state->stroke_color);
        _wagner_line(c, (int)fx2, (int)fy2, (int)fx3, (int)fy3, state->stroke_color);
        _wagner_line(c, (int)fx3, (int)fy3, (int)fx1, (int)fy1, state->stroke_color);
    }
}

void triangle(void) {
    Canvas c = _wagner_get_target(); triangle_pts(0, -1, 0.866025f, 0.5f, -0.866025f, 0.5f); }

void line(float x1, float y1, float x2, float y2) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    if (!state->has_stroke) return;
    float fx1 = x1, fy1 = y1; _wagner_transform(&fx1, &fy1);
    float fx2 = x2, fy2 = y2; _wagner_transform(&fx2, &fy2);
    _wagner_line(c, (int)fx1, (int)fy1, (int)fx2, (int)fy2, state->stroke_color);
}

void pixel(float x, float y) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    if (!state->has_fill) return;
    float fx = x, fy = y; _wagner_transform(&fx, &fy);
    _wagner_set_pixel_raw(c, (int)fx, (int)fy, state->fill_color);
}

pixel_t pixel_at(Canvas c, int x, int y) {
    return _wagner_get_pixel_raw(c, x, y);
}

void triangle3uv(
    int x1, int y1, int x2, int y2, int x3, int y3,
    float tx1, float ty1, float tx2, float ty2, float tx3, float ty3,
    float z1, float z2, float z3, Canvas texture)
{
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    float fx1 = x1, fy1 = y1; _wagner_transform(&fx1, &fy1);
    float fx2 = x2, fy2 = y2; _wagner_transform(&fx2, &fy2);
    float fx3 = x3, fy3 = y3; _wagner_transform(&fx3, &fy3);
    _wagner_triangle_textured(c, (int)fx1, (int)fy1, (int)fx2, (int)fy2, (int)fx3, (int)fy3,
        tx1, ty1, tx2, ty2, tx3, ty3,
        texture, state->has_fill, state->fill_color);
}

// ============================================
// TEXT FUNCTIONS IMPLEMENTATION
// ============================================

void text(const char* text_str) {
    Canvas c = _wagner_get_target();
    WagnerRenderState* state = _wagner_current_state();
    if (!state->has_fill) return;
    
    float fx = 0, fy = 0; _wagner_transform(&fx, &fy);
    WagnerMatrix* m = _wagner_current_matrix();
    int size = (int)wabs(m->a);
    if (size < 1) size = 1;
    
    char buf[256];
    const char* src = text_str;
    char* dst = buf;
    while (*src && dst - buf < 255) {
        char ch = *src++;
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        *dst++ = ch;
    }
    *dst = 0;
    _wagner_text(c, buf, (int)fx, (int)fy, size, state->fill_color);
}

int text_width(const char* text_str) {
    int len = 0;
    while (text_str[len]) len++;
    WagnerMatrix* m = _wagner_current_matrix();
    int size = (int)wabs(m->a);
    if (size < 1) size = 1;
    return len * 8 * size;
}

// ============================================
// IMAGE FUNCTIONS IMPLEMENTATION
// ============================================

Canvas canvas_create(int w, int h, int bpp) {
    void* px = malloc(w * h * ((bpp + 7) / 8));
    if (!px) return (Canvas){0};
    Canvas c = { px, w, h, w, (uint8_t)bpp, 
                 (uint8_t)WAGNER_CFG_R_BITS, (uint8_t)WAGNER_CFG_R_SHIFT,
                 (uint8_t)WAGNER_CFG_G_BITS, (uint8_t)WAGNER_CFG_G_SHIFT,
                 (uint8_t)WAGNER_CFG_B_BITS, (uint8_t)WAGNER_CFG_B_SHIFT,
                 (uint8_t)WAGNER_CFG_A_BITS, (uint8_t)WAGNER_CFG_A_SHIFT };
    return c;
}

Image create_image(int width, int height) {
    return canvas_create(width, height, w_bpp);
}

Canvas img_create(const void* data, int width, int height, int bpp) {
    Canvas img = { (void*)data, width, height, width, (uint8_t)bpp,
                   (uint8_t)WAGNER_CFG_R_BITS, (uint8_t)WAGNER_CFG_R_SHIFT,
                   (uint8_t)WAGNER_CFG_G_BITS, (uint8_t)WAGNER_CFG_G_SHIFT,
                   (uint8_t)WAGNER_CFG_B_BITS, (uint8_t)WAGNER_CFG_B_SHIFT,
                   (uint8_t)WAGNER_CFG_A_BITS, (uint8_t)WAGNER_CFG_A_SHIFT };
    return img;
 }


static inline pixel_t lerp_color(pixel_t a, pixel_t b, float t) {
    uint8_t ar = 0, ag = 0, ab_ = 0, aa = 255;
    uint8_t br = 0, bg = 0, bb_ = 0, ba = 255;
    
    if (!WAGNER_CFG_R_BITS && !WAGNER_CFG_G_BITS && !WAGNER_CFG_B_BITS && WAGNER_CFG_A_BITS) {
        ar = ag = ab_ = (uint8_t)(((a >> WAGNER_CFG_A_SHIFT) & ((1ULL << WAGNER_CFG_A_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_A_BITS) - 1));
        br = bg = bb_ = (uint8_t)(((b >> WAGNER_CFG_A_SHIFT) & ((1ULL << WAGNER_CFG_A_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_A_BITS) - 1));
    } else {
        if (WAGNER_CFG_R_BITS) {
            ar = (uint8_t)(((a >> WAGNER_CFG_R_SHIFT) & ((1ULL << WAGNER_CFG_R_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_R_BITS) - 1));
            br = (uint8_t)(((b >> WAGNER_CFG_R_SHIFT) & ((1ULL << WAGNER_CFG_R_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_R_BITS) - 1));
        }
        if (WAGNER_CFG_G_BITS) {
            ag = (uint8_t)(((a >> WAGNER_CFG_G_SHIFT) & ((1ULL << WAGNER_CFG_G_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_G_BITS) - 1));
            bg = (uint8_t)(((b >> WAGNER_CFG_G_SHIFT) & ((1ULL << WAGNER_CFG_G_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_G_BITS) - 1));
        }
        if (WAGNER_CFG_B_BITS) {
            ab_ = (uint8_t)(((a >> WAGNER_CFG_B_SHIFT) & ((1ULL << WAGNER_CFG_B_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_B_BITS) - 1));
            bb_ = (uint8_t)(((b >> WAGNER_CFG_B_SHIFT) & ((1ULL << WAGNER_CFG_B_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_B_BITS) - 1));
        }
        if (WAGNER_CFG_A_BITS) {
            aa = (uint8_t)(((a >> WAGNER_CFG_A_SHIFT) & ((1ULL << WAGNER_CFG_A_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_A_BITS) - 1));
            ba = (uint8_t)(((b >> WAGNER_CFG_A_SHIFT) & ((1ULL << WAGNER_CFG_A_BITS) - 1)) * 255 / ((1ULL << WAGNER_CFG_A_BITS) - 1));
        }
    }
    
    uint8_t r = (uint8_t)(ar + (br - ar) * t);
    uint8_t g = (uint8_t)(ag + (bg - ag) * t);
    uint8_t bv = (uint8_t)(ab_ + (bb_ - ab_) * t);
    uint8_t av = (uint8_t)(aa + (ba - aa) * t);
    
    return rgba(r, g, bv, av);
}

static inline int text_height(void) {
    WagnerMatrix* m = _wagner_current_matrix();
    int size = (int)wabs(m->d);
    if (size < 1) size = 1;
    return 8 * size;
}



// ============================================
// DECODER WRAPPERS & ASSET CACHE
// ============================================

#ifndef WAGNER_NO_PNG_DECODE
unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h,
                         const unsigned char* in, size_t insize);

Canvas img_load(const uint8_t* data, size_t size) {
    uint8_t* decoded = 0;
    unsigned w, h;
    if (lodepng_decode32(&decoded, &w, &h, data, size)) return (Canvas){0};
    Canvas c = { .pixels = decoded, .width = (int)w, .height = (int)h, .stride = (int)w, .bpp = 32,
                 .r_bits = 8, .r_shift = 0,
                 .g_bits = 8, .g_shift = 8,
                 .b_bits = 8, .b_shift = 16,
                 .a_bits = 8, .a_shift = 24 };
    return c;
}
#endif

#ifndef WAGNER_NO_GIF_DECODE
#define STBI_NO_STDIO
#define STBI_NO_JPEG
#define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_HDR
#define STBI_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Image gif_load(const uint8_t* data, size_t size) {
    int w = 0, h = 0, comp = 0;
    stbi_uc *pixels = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    if (!pixels) return (Image){0};
    Image img = { .pixels = pixels, .width = w, .height = h, .stride = w, .bpp = 32,
                  .r_bits = 8, .r_shift = 0,
                  .g_bits = 8, .g_shift = 8,
                  .b_bits = 8, .b_shift = 16,
                  .a_bits = 8, .a_shift = 24 };
    return img;
}

Gif gif_load_anim(const uint8_t* data, size_t size) {
    Gif g = {0};
    int w = 0, h = 0, z = 0, comp = 0;
    int *delays = NULL;
    stbi_uc *pixels = stbi_load_gif_from_memory(data, (int)size, &delays, &w, &h, &z, &comp, 4);
    if (!pixels || z <= 0) return g;
    
    g.width = w;
    g.height = h;
    g.frame_count = z;
    g.delays = delays;
    g.frames = (Image*)malloc(sizeof(Image) * (size_t)z);
    
    size_t frame_bytes = (size_t)(w * h * 4);
    for (int i = 0; i < z; i++) {
        g.frames[i] = (Image){
            .pixels = pixels + (i * frame_bytes),
            .width = w,
            .height = h,
            .stride = w,
            .bpp = 32,
            .r_bits = 8, .r_shift = 0,
            .g_bits = 8, .g_shift = 8,
            .b_bits = 8, .b_shift = 16,
            .a_bits = 8, .a_shift = 24
        };
    }
    return g;
}
#endif

// ============================================
// ASSET LOADERS WITH CACHING
// ============================================

#include "assets.h"

typedef struct {
    const char* path;
    Image image;
    int loaded;
} _WagnerImageCacheEntry;

#define _WAGNER_MAX_CACHE_ENTRIES 64
static _WagnerImageCacheEntry _wagner_img_cache[_WAGNER_MAX_CACHE_ENTRIES];
static int _wagner_img_cache_count = 0;

static const WagnerAsset* _wagner_find_asset(const char* path) {
    if (!path) return NULL;
    for (int i = 0; i < WAGNER_ASSET_COUNT; i++) {
        const char* a = WAGNER_ASSETS[i].path;
        const char* b = path;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return &WAGNER_ASSETS[i];
    }
    const char* base = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    for (int i = 0; i < WAGNER_ASSET_COUNT; i++) {
        const char* a = WAGNER_ASSETS[i].path;
        const char* b = base;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return &WAGNER_ASSETS[i];
    }
    return NULL;
}

Image raw_image_load(const uint8_t* data, size_t size) {
    if (!data || size < 13) return (Image){0};
    if (data[0] == 'W' && data[1] == 'R' && data[2] == 'A' && data[3] == 'W' && data[4] == 'I') {
        uint32_t w = *(uint32_t*)(data + 5);
        uint32_t h = *(uint32_t*)(data + 9);
        const uint8_t* pixels = data + 13;
        return (Image){
            .pixels = (void*)pixels,
            .width = (int)w,
            .height = (int)h,
            .stride = (int)w,
            .bpp = 32,
            .r_bits = 8, .r_shift = 0,
            .g_bits = 8, .g_shift = 8,
            .b_bits = 8, .b_shift = 16,
            .a_bits = 8, .a_shift = 24
        };
    }
    return (Image){0};
}

Gif raw_video_load(const uint8_t* data, size_t size) {
    Gif g = {0};
    if (!data || size < 21) return g;
    if (data[0] == 'W' && data[1] == 'R' && data[2] == 'A' && data[3] == 'W' && data[4] == 'V') {
        uint32_t w = *(uint32_t*)(data + 5);
        uint32_t h = *(uint32_t*)(data + 9);
        uint32_t z = *(uint32_t*)(data + 13);
        uint32_t fps = *(uint32_t*)(data + 17);
        const uint8_t* raw_frames = data + 21;

        g.width = (int)w;
        g.height = (int)h;
        g.frame_count = (int)z;
        g.frames = (Image*)malloc(sizeof(Image) * (size_t)z);
        g.delays = (int*)malloc(sizeof(int) * (size_t)z);

        int delay_ms = fps > 0 ? (1000 / (int)fps) : 33;
        size_t frame_bytes = (size_t)(w * h * 4);

        for (uint32_t i = 0; i < z; i++) {
            g.delays[i] = delay_ms;
            g.frames[i] = (Image){
                .pixels = (void*)(raw_frames + (i * frame_bytes)),
                .width = (int)w,
                .height = (int)h,
                .stride = (int)w,
                .bpp = 32,
                .r_bits = 8, .r_shift = 0,
                .g_bits = 8, .g_shift = 8,
                .b_bits = 8, .b_shift = 16,
                .a_bits = 8, .a_shift = 24
            };
        }
    }
    return g;
}

Image load_image(const char* path) {
    if (!path) return (Image){0};
    for (int i = 0; i < _wagner_img_cache_count; i++) {
        const char* a = _wagner_img_cache[i].path;
        const char* b = path;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == 0 && *b == 0) return _wagner_img_cache[i].image;
    }

    const WagnerAsset* asset = _wagner_find_asset(path);
    if (asset) {
        Image img = raw_image_load((uint8_t*)asset->data, asset->size);
#ifndef WAGNER_NO_PNG_DECODE
        if (!img.pixels) img = img_load((uint8_t*)asset->data, asset->size);
#endif
#ifndef WAGNER_NO_GIF_DECODE
        if (!img.pixels) img = gif_load((uint8_t*)asset->data, asset->size);
#endif
        if (img.pixels && _wagner_img_cache_count < _WAGNER_MAX_CACHE_ENTRIES) {
            _wagner_img_cache[_wagner_img_cache_count].path = path;
            _wagner_img_cache[_wagner_img_cache_count].image = img;
            _wagner_img_cache[_wagner_img_cache_count].loaded = 1;
            _wagner_img_cache_count++;
        }
        return img;
    }
    return (Image){0};
}

Gif load_gif_anim(const char* path) {
    if (!path) return (Gif){0};
    const WagnerAsset* asset = _wagner_find_asset(path);
    if (asset) {
        Gif g = raw_video_load((uint8_t*)asset->data, asset->size);
        if (g.frames) return g;
#ifndef WAGNER_NO_GIF_DECODE
        return gif_load_anim((uint8_t*)asset->data, asset->size);
#endif
    }
    return (Gif){0};
}

Data load_data(const char* path) {
    Data out = {0};
    const WagnerAsset* asset = _wagner_find_asset(path);
    if (asset) {
        out.data = (void*)asset->data;
        out.size = asset->size;
    }
    return out;
}

// Save removed in Wagnostic 2.0

static int _wagner_skip_frame = 0;

static inline void no_draw(void) { _wagner_skip_frame = 1; }

int wupdate() {
    static int init = 0;
    _wagner_frame_counter++;
    if (!init) {
        init = 1;
        _wagner_keys_ptr = (uint8_t*)wextension("std:keyboard", NULL);
        _wagner_mouse_ptr = (WagnosticMouseState*)wextension("std:mouse", NULL);
        _wagner_gamepad_ptr = (uint32_t*)wextension("std:gamepad", NULL);

        wagner.width  = 320; wagner.height = 240;
        wagner.bpp    = 16;  wagner.scale  = 4;
        wagner.frame_count = 0; wagner.fps = 0;
        wagner.delta_time = 0.016f;
        wagner.mouse  = vec2(0, 0); wagner.pmouse = vec2(0, 0);
        wagner.mouse_pressed = false; wagner.mouse_released = false;
        wagner.mouse_down = false;
        
        _wagner_rom.state.vram_offset = (uint32_t)((uint8_t*)_wagner_rom.vram - (uint8_t*)&_wagner_rom.state);
        _wagner_rom.state.r_bits = WAGNER_CFG_R_BITS;
        _wagner_rom.state.r_shift = WAGNER_CFG_R_SHIFT;
        _wagner_rom.state.g_bits = WAGNER_CFG_G_BITS;
        _wagner_rom.state.g_shift = WAGNER_CFG_G_SHIFT;
        _wagner_rom.state.b_bits = WAGNER_CFG_B_BITS;
        _wagner_rom.state.b_shift = WAGNER_CFG_B_SHIFT;
        _wagner_rom.state.a_bits = WAGNER_CFG_A_BITS;
        _wagner_rom.state.a_shift = WAGNER_CFG_A_SHIFT;
        wagner.canvas_pixels = w_vram;
        screen.pixels = w_vram; screen.width = w_width; screen.height = w_height;
        screen.stride = w_width; screen.bpp = WAGNER_CFG_BPP;
        screen.r_bits = WAGNER_CFG_R_BITS; screen.r_shift = WAGNER_CFG_R_SHIFT;
        screen.g_bits = WAGNER_CFG_G_BITS; screen.g_shift = WAGNER_CFG_G_SHIFT;
        screen.b_bits = WAGNER_CFG_B_BITS; screen.b_shift = WAGNER_CFG_B_SHIFT;
        screen.a_bits = WAGNER_CFG_A_BITS; screen.a_shift = WAGNER_CFG_A_SHIFT;
        w_setup(&_wagner_rom.state, WAGNER_TITLE, WAGNER_CFG_W, WAGNER_CFG_H, WAGNER_CFG_BPP, WAGNER_CFG_SCALE);
        wagner.width = w_width; wagner.height = w_height;
        wagner.bpp = w_bpp; wagner.scale = w_scale;
        screen.width = w_width; screen.height = w_height;
        screen.stride = w_width; screen.bpp = (uint8_t)w_bpp;
    }

    // Input processing
    wagner.pmouse = wagner.mouse;
    wagner.mouse = vec2(w_mouse_x, w_mouse_y);
    bool cur = (w_mouse_buttons & 1) != 0;
    wagner.mouse_pressed = cur && !wagner.mouse_down;
    wagner.mouse_released = !cur && wagner.mouse_down;
    wagner.mouse_down = cur;
    wagner.mouse_button = cur ? 1 : 0;
    for (int i = 0; i < 256; i++) {
        bool k = w_keys ? (w_keys[i] != 0) : false;
        wagner.keys_pressed[i] = k && !wagner.keys[i];
        wagner.keys_released[i] = !k && wagner.keys[i];
        wagner.keys[i] = k;
    }

    static int preloaded = 0;
    if (!preloaded) {
        preloaded = 1;
        if (preload) preload();
        if (setup) setup();
    }

    // Regular draw loop
    static uint32_t last_ticks = 0;
    uint32_t now = w_ticks;
    if (last_ticks > 0) wagner.delta_time = (now - last_ticks) / 1000.0f;
    last_ticks = now; wagner.frame_count++;
    
    draw();
    
    static uint32_t _fps_timer = 0;
    if (_fps_timer == 0) _fps_timer = now;
    uint32_t _fps_elapsed = now - _fps_timer;
    if (_fps_elapsed >= 1000) {
        wagner.fps = (wagner.frame_count * 1000) / (_fps_elapsed ? _fps_elapsed : 1);
        wagner.frame_count = 0;
        _fps_timer = now;
    }
    
    if (_wagner_skip_frame) {
        _wagner_skip_frame = 0;
        w_no_redraw(&_wagner_rom.state, &_wagner_rom.dirty_list);
    } else {
        w_redraw(&_wagner_rom.state, &_wagner_rom.dirty_list);
    }
    return (int)&_wagner_rom.state;
}

#endif // WAGNER_H