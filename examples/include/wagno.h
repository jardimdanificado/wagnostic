#ifndef WAGNO_H
#define WAGNO_H

/**
 * WagnO - Easy Game Development API for Wagnostic
 * 
 * Inspired by p5.js and LÖVE2D, this API provides a simple way to create
 * games and interactive applications for the Wagnostic WASM runtime.
 * 
 * Usage:
 *   #define WAGNO_IMPLEMENTATION
 *   #include "wagno.h"
 * 
 *   void setup() { ... }
 *   void update() { ... }
 *   void draw() { ... }
 */

#include <stdint.h>
#include <stdbool.h>

// ============================================
// WAGNOSTIC COMPATIBILITY MACROS
// ============================================

#ifndef W_RGB565
#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#endif

#ifndef W_RGBA
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
#endif

#ifndef W_RGB332
#define W_RGB332(r, g, b) (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))
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

typedef struct {
    int x, y, w, h;
} Rect;

// ============================================
// GLOBAL STATE (managed by WagnO)
// ============================================

static struct {
    // Screen
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
    
    // Graphics
    Color fill_color;
    Color stroke_color;
    int stroke_weight;
    bool no_fill;
    bool no_stroke;
    
    // Canvas pointer (internal)
    void* canvas_pixels;
} wagno;

// ============================================
// COLOR CONSTANTS
// ============================================

#define WAGNO_BLACK   ((Color){0, 0, 0, 255})
#define WAGNO_WHITE   ((Color){255, 255, 255, 255})
#define WAGNO_RED     ((Color){255, 0, 0, 255})
#define WAGNO_GREEN   ((Color){0, 255, 0, 255})
#define WAGNO_BLUE    ((Color){0, 0, 255, 255})
#define WAGNO_YELLOW  ((Color){255, 255, 0, 255})
#define WAGNO_CYAN    ((Color){0, 255, 255, 255})
#define WAGNO_MAGENTA ((Color){255, 0, 255, 255})
#define WAGNO_GRAY    ((Color){128, 128, 128, 255})
#define WAGNO_ORANGE  ((Color){255, 165, 0, 255})
#define WAGNO_PURPLE  ((Color){128, 0, 128, 255})

// ============================================
// MATH UTILITIES
// ============================================

#define WAGNO_PI 3.14159265358979f
#define WAGNO_TWO_PI 6.28318530717959f
#define WAGNO_HALF_PI 1.5707963267949f

static inline float wagno_map(float value, float start1, float stop1, float start2, float stop2) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

static inline float wagno_constrain(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static inline float wagno_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float wagno_dist(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return dx * dx + dy * dy;  // squared distance for speed
}

static inline float wagno_sqrt(float x) {
    // Newton's method approximation
    if (x <= 0) return 0;
    float guess = x / 2.0f;
    for (int i = 0; i < 10; i++) {
        guess = (guess + x / guess) / 2.0f;
    }
    return guess;
}

static inline float wagno_abs(float x) {
    return x < 0 ? -x : x;
}

static inline float wagno_min(float a, float b) {
    return a < b ? a : b;
}

static inline float wagno_max(float a, float b) {
    return a > b ? a : b;
}

// Trigonometric functions using Bhaskara I approximation
static inline float wagno_sin(float x) {
    // Normalize to [0, 2π]
    while (x < 0) x += WAGNO_TWO_PI;
    while (x >= WAGNO_TWO_PI) x -= WAGNO_TWO_PI;
    
    if (x > WAGNO_PI) {
        float y = x - WAGNO_PI;
        return -16.0f * y * (WAGNO_PI - y) / (5.0f * WAGNO_PI * WAGNO_PI - 4.0f * y * (WAGNO_PI - y));
    }
    return 16.0f * x * (WAGNO_PI - x) / (5.0f * WAGNO_PI * WAGNO_PI - 4.0f * x * (WAGNO_PI - x));
}

static inline float wagno_cos(float x) {
    return wagno_sin(x + WAGNO_HALF_PI);
}

static inline int wagno_random_int(int min_val, int max_val) {
    // Simple LCG random
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    return min_val + (seed >> 16) % (max_val - min_val + 1);
}

static inline float wagno_random(float min_val, float max_val) {
    return min_val + (float)wagno_random_int(0, 10000) / 10000.0f * (max_val - min_val);
}

// ============================================
// COLOR FUNCTIONS
// ============================================

static inline Color wagno_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    Color c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = 255;
    return c;
}

static inline Color wagno_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Color c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static inline Color wagno_color_hex(uint32_t hex) {
    Color c;
    c.r = (hex >> 16) & 0xFF;
    c.g = (hex >> 8) & 0xFF;
    c.b = hex & 0xFF;
    c.a = 255;
    return c;
}

static inline uint16_t wagno_color_to_rgb565(Color c) {
    return W_RGB565(c.r, c.g, c.b);
}

// ============================================
// VECTOR FUNCTIONS
// ============================================

static inline Vec2 wagno_vec2(float x, float y) {
    return (Vec2){x, y};
}

static inline Vec2 wagno_vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

static inline Vec2 wagno_vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

static inline Vec2 wagno_vec2_mul(Vec2 v, float s) {
    return (Vec2){v.x * s, v.y * s};
}

static inline float wagno_vec2_len(Vec2 v) {
    return wagno_sqrt(v.x * v.x + v.y * v.y);
}

static inline Vec2 wagno_vec2_normalize(Vec2 v) {
    float len = wagno_vec2_len(v);
    if (len == 0) return (Vec2){0, 0};
    return (Vec2){v.x / len, v.y / len};
}

// ============================================
// DRAWING STATE FUNCTIONS
// ============================================

static inline void wagno_fill(Color c) {
    wagno.fill_color = c;
    wagno.no_fill = false;
}

static inline void wagno_no_fill() {
    wagno.no_fill = true;
}

static inline void wagno_stroke(Color c) {
    wagno.stroke_color = c;
    wagno.no_stroke = false;
}

static inline void wagno_no_stroke() {
    wagno.no_stroke = true;
}

static inline void wagno_stroke_weight(int w) {
    wagno.stroke_weight = w;
}

// ============================================
// DRAWING PRIMITIVES (declared, implemented below)
// ============================================

void wagno_background(Color c);
void wagno_rect(int x, int y, int w, int h);
void wagno_rect_mode(int mode);  // 0=CORNER, 1=CENTER
void wagno_ellipse(int x, int y, int w, int h);
void wagno_line(int x1, int y1, int x2, int y2);
void wagno_point(int x, int y);
void wagno_triangle(int x1, int y1, int x2, int y2, int x3, int y3);
void wagno_quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
void wagno_arc(int x, int y, int w, int h, float start, float stop);

// ============================================
// TEXT FUNCTIONS
// ============================================

void wagno_text(const char* text, int x, int y);
void wagno_text_size(int size);
int wagno_text_width(const char* text);

// ============================================
// IMAGE FUNCTIONS
// ============================================

typedef struct {
    void* pixels;
    int width;
    int height;
    int bpp;  // 8, 16, or 32
} WagnoImage;

WagnoImage wagno_create_image(int width, int height, int bpp);
WagnoImage wagno_create_image_from_data(const void* data, int width, int height, int bpp);
void wagno_image(WagnoImage img, int x, int y);
void wagno_image_scaled(WagnoImage img, int x, int y, int w, int h);
void wagno_load_image(WagnoImage* img, const void* data, int width, int height, int bpp);

// ============================================
// AUDIO FUNCTIONS
// ============================================

void wagno_play_tone(float freq, float duration, float volume);
void wagno_play_noise(float duration, float volume);

// ============================================
// USER FUNCTIONS (must be implemented by user)
// ============================================

void setup(void);
void update(void);
void draw(void);

// Optional user functions
void mouse_pressed(void);
void mouse_released(void);
void key_pressed(int key);
void key_released(int key);

// ============================================
// WAGNO_IMPLEMENTATION
// ============================================

#ifdef WAGNO_IMPLEMENTATION

#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

static Olivec_Canvas _wagno_canvas;
static int _wagno_rect_mode = 0;  // 0=CORNER, 1=CENTER

// Internal drawing functions
static inline void _wagno_set_pixel(int x, int y, Color c) {
    if (x < 0 || x >= wagno.width || y < 0 || y >= wagno.height) return;
    uint16_t* pixels = (uint16_t*)wagno.canvas_pixels;
    pixels[y * wagno.width + x] = wagno_color_to_rgb565(c);
}

static void _wagno_draw_filled_rect(int x, int y, int w, int h, Color c) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            _wagno_set_pixel(ix, iy, c);
        }
    }
}

static void _wagno_draw_rect_outline(int x, int y, int w, int h, Color c, int weight) {
    for (int i = 0; i < weight; i++) {
        // Top
        for (int ix = x + i; ix < x + w - i; ix++) {
            _wagno_set_pixel(ix, y + i, c);
        }
        // Bottom
        for (int ix = x + i; ix < x + w - i; ix++) {
            _wagno_set_pixel(ix, y + h - 1 - i, c);
        }
        // Left
        for (int iy = y + i; iy < y + h - i; iy++) {
            _wagno_set_pixel(x + i, iy, c);
        }
        // Right
        for (int iy = y + i; iy < y + h - i; iy++) {
            _wagno_set_pixel(x + w - 1 - i, iy, c);
        }
    }
}

// ============================================
// DRAWING PRIMITIVES IMPLEMENTATION
// ============================================

void wagno_background(Color c) {
    _wagno_draw_filled_rect(0, 0, wagno.width, wagno.height, c);
}

void wagno_rect(int x, int y, int w, int h) {
    if (_wagno_rect_mode == 1) {  // CENTER
        x -= w / 2;
        y -= h / 2;
    }
    
    if (!wagno.no_fill) {
        _wagno_draw_filled_rect(x, y, w, h, wagno.fill_color);
    }
    if (!wagno.no_stroke) {
        _wagno_draw_rect_outline(x, y, w, h, wagno.stroke_color, wagno.stroke_weight);
    }
}

void wagno_rect_mode(int mode) {
    _wagno_rect_mode = mode;
}

void wagno_ellipse(int x, int y, int w, int h) {
    int rx = w / 2;
    int ry = h / 2;
    
    // Draw filled ellipse
    for (int iy = -ry; iy <= ry; iy++) {
        for (int ix = -rx; ix <= rx; ix++) {
            float nx = (float)ix / rx;
            float ny = (float)iy / ry;
            if (nx * nx + ny * ny <= 1.0f) {
                if (!wagno.no_fill) {
                    _wagno_set_pixel(x + ix, y + iy, wagno.fill_color);
                }
            }
        }
    }
    
    if (!wagno.no_stroke) {
        // Draw outline
        for (int angle = 0; angle < 360; angle++) {
            float rad = angle * WAGNO_PI / 180.0f;
            int px = x + (int)(rx * wagno_cos(rad));
            int py = y + (int)(ry * wagno_sin(rad));
            _wagno_set_pixel(px, py, wagno.stroke_color);
        }
    }
}

void wagno_line(int x1, int y1, int x2, int y2) {
    // Bresenham's line algorithm
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = dx > dy ? dx : dy;
    if (steps == 0) steps = 1;
    
    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;
    
    float x = x1;
    float y = y1;
    
    for (int i = 0; i <= steps; i++) {
        _wagno_set_pixel((int)x, (int)y, wagno.stroke_color);
        x += x_inc;
        y += y_inc;
    }
}

void wagno_point(int x, int y) {
    _wagno_set_pixel(x, y, wagno.stroke_color);
}

void wagno_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    if (!wagno.no_fill) {
        // Scanline fill
        int min_y = y1 < y2 ? (y1 < y3 ? y1 : y3) : (y2 < y3 ? y2 : y3);
        int max_y = y1 > y2 ? (y1 > y3 ? y1 : y3) : (y2 > y3 ? y2 : y3);
        
        for (int y = min_y; y <= max_y; y++) {
            int x_min = wagno.width, x_max = 0;
            
            // Check edges with zero-division protection
            if (y >= y1 && y <= y2 && y2 != y1) {
                int x = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
                if (x < x_min) x_min = x;
                if (x > x_max) x_max = x;
            }
            if (y >= y2 && y <= y3 && y3 != y2) {
                int x = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
                if (x < x_min) x_min = x;
                if (x > x_max) x_max = x;
            }
            if (y >= y3 && y <= y1 && y1 != y3) {
                int x = x3 + (x1 - x3) * (y - y3) / (y1 - y3);
                if (x < x_min) x_min = x;
                if (x > x_max) x_max = x;
            }
            
            for (int x = x_min; x <= x_max; x++) {
                _wagno_set_pixel(x, y, wagno.fill_color);
            }
        }
    }
    
    if (!wagno.no_stroke) {
        wagno_line(x1, y1, x2, y2);
        wagno_line(x2, y2, x3, y3);
        wagno_line(x3, y3, x1, y1);
    }
}

void wagno_quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    if (!wagno.no_stroke) {
        wagno_line(x1, y1, x2, y2);
        wagno_line(x2, y2, x3, y3);
        wagno_line(x3, y3, x4, y4);
        wagno_line(x4, y4, x1, y1);
    }
}

void wagno_arc(int x, int y, int w, int h, float start, float stop) {
    // Simple arc implementation
    int rx = w / 2;
    int ry = h / 2;
    
    for (float angle = start; angle < stop; angle += 0.01f) {
        int px = x + (int)(rx * wagno_cos(angle));
        int py = y + (int)(ry * wagno_sin(angle));
        _wagno_set_pixel(px, py, wagno.stroke_color);
    }
}

// ============================================
// TEXT FUNCTIONS IMPLEMENTATION
// ============================================

void wagno_text(const char* text, int x, int y) {
    // Simple bitmap font rendering
    // For now, just draw a placeholder rectangle for each character
    int char_width = 6;
    int char_height = 8;
    
    for (int i = 0; text[i] != '\0'; i++) {
        int cx = x + i * char_width;
        if (!wagno.no_fill) {
            _wagno_draw_filled_rect(cx, y, char_width - 1, char_height - 1, wagno.fill_color);
        }
    }
}

void wagno_text_size(int size) {
    // Placeholder - would need actual font support
}

int wagno_text_width(const char* text) {
    int len = 0;
    while (text[len]) len++;
    return len * 6;  // assuming 6px per character
}

// ============================================
// IMAGE FUNCTIONS IMPLEMENTATION
// ============================================

WagnoImage wagno_create_image(int width, int height, int bpp) {
    WagnoImage img;
    img.width = width;
    img.height = height;
    img.bpp = bpp;
    img.pixels = NULL;
    return img;
}

WagnoImage wagno_create_image_from_data(const void* data, int width, int height, int bpp) {
    WagnoImage img;
    img.width = width;
    img.height = height;
    img.bpp = bpp;
    img.pixels = (void*)data;
    return img;
}

void wagno_image(WagnoImage img, int x, int y) {
    if (!img.pixels) return;
    
    // Draw image pixel by pixel
    for (int iy = 0; iy < img.height; iy++) {
        for (int ix = 0; ix < img.width; ix++) {
            int px = x + ix;
            int py = y + iy;
            
            if (px < 0 || px >= wagno.width || py < 0 || py >= wagno.height) continue;
            
            Color c;
            if (img.bpp == 32) {
                uint32_t* pixels = (uint32_t*)img.pixels;
                uint32_t p = pixels[iy * img.width + ix];
                c.r = p & 0xFF;
                c.g = (p >> 8) & 0xFF;
                c.b = (p >> 16) & 0xFF;
                c.a = (p >> 24) & 0xFF;
            } else if (img.bpp == 16) {
                uint16_t* pixels = (uint16_t*)img.pixels;
                uint16_t p = pixels[iy * img.width + ix];
                c.r = ((p >> 11) & 0x1F) * 255 / 31;
                c.g = ((p >> 5) & 0x3F) * 255 / 63;
                c.b = (p & 0x1F) * 255 / 31;
                c.a = 255;
            } else {
                uint8_t* pixels = (uint8_t*)img.pixels;
                uint8_t p = pixels[iy * img.width + ix];
                c.r = ((p >> 5) & 0x07) * 255 / 7;
                c.g = ((p >> 2) & 0x07) * 255 / 7;
                c.b = (p & 0x03) * 255 / 3;
                c.a = 255;
            }
            
            if (c.a > 128) {  // Simple alpha test
                _wagno_set_pixel(px, py, c);
            }
        }
    }
}

void wagno_image_scaled(WagnoImage img, int x, int y, int w, int h) {
    if (!img.pixels) return;
    
    // Simple nearest-neighbor scaling
    for (int iy = 0; iy < h; iy++) {
        for (int ix = 0; ix < w; ix++) {
            int src_x = ix * img.width / w;
            int src_y = iy * img.height / h;
            
            int px = x + ix;
            int py = y + iy;
            
            if (px < 0 || px >= wagno.width || py < 0 || py >= wagno.height) continue;
            
            Color c;
            if (img.bpp == 32) {
                uint32_t* pixels = (uint32_t*)img.pixels;
                uint32_t p = pixels[src_y * img.width + src_x];
                c.r = p & 0xFF;
                c.g = (p >> 8) & 0xFF;
                c.b = (p >> 16) & 0xFF;
                c.a = (p >> 24) & 0xFF;
            } else if (img.bpp == 16) {
                uint16_t* pixels = (uint16_t*)img.pixels;
                uint16_t p = pixels[src_y * img.width + src_x];
                c.r = ((p >> 11) & 0x1F) * 255 / 31;
                c.g = ((p >> 5) & 0x3F) * 255 / 63;
                c.b = (p & 0x1F) * 255 / 31;
                c.a = 255;
            } else {
                uint8_t* pixels = (uint8_t*)img.pixels;
                uint8_t p = pixels[src_y * img.width + src_x];
                c.r = ((p >> 5) & 0x07) * 255 / 7;
                c.g = ((p >> 2) & 0x07) * 255 / 7;
                c.b = (p & 0x03) * 255 / 3;
                c.a = 255;
            }
            
            if (c.a > 128) {
                _wagno_set_pixel(px, py, c);
            }
        }
    }
}

// ============================================
// AUDIO FUNCTIONS (placeholders)
// ============================================

void wagno_play_tone(float freq, float duration, float volume) {
    // Would need to write to audio buffer
    // Placeholder for now
}

void wagno_play_noise(float duration, float volume) {
    // Placeholder
}

// ============================================
// MAIN WAGNO FUNCTIONS
// ============================================

__attribute__((visibility("default")))
void winit() {
    // Initialize WagnO state
    wagno.width = 320;
    wagno.height = 240;
    wagno.bpp = 16;
    wagno.scale = 4;
    wagno.frame_count = 0;
    wagno.fps = 0;
    wagno.delta_time = 0.016f;  // ~60fps
    
    wagno.fill_color = WAGNO_WHITE;
    wagno.stroke_color = WAGNO_BLACK;
    wagno.stroke_weight = 1;
    wagno.no_fill = false;
    wagno.no_stroke = false;
    
    wagno.mouse = wagno_vec2(0, 0);
    wagno.pmouse = wagno_vec2(0, 0);
    wagno.mouse_pressed = false;
    wagno.mouse_released = false;
    wagno.mouse_down = false;
    
    // Setup Wagnostic
    w_setup("WagnO Game", wagno.width, wagno.height, wagno.bpp, wagno.scale, 0);
    wagno.canvas_pixels = W_FB_PTR;
    
    // Call user setup
    setup();
}

__attribute__((visibility("default")))
void wupdate() {
    // Update time
    static uint32_t last_ticks = 0;
    uint32_t current_ticks = W_SYS->ticks;
    if (last_ticks > 0) {
        wagno.delta_time = (current_ticks - last_ticks) / 1000.0f;
    }
    last_ticks = current_ticks;
    wagno.frame_count++;
    
    // Update input state
    wagno.pmouse = wagno.mouse;
    wagno.mouse = wagno_vec2(W_SYS->mouse_x, W_SYS->mouse_y);
    
    // Detect mouse press/release
    bool current_mouse_down = (W_SYS->mouse_buttons & 1) != 0;
    wagno.mouse_pressed = current_mouse_down && !wagno.mouse_down;
    wagno.mouse_released = !current_mouse_down && wagno.mouse_down;
    wagno.mouse_down = current_mouse_down;
    wagno.mouse_button = current_mouse_down ? 1 : 0;
    
    // Detect key press/release
    for (int i = 0; i < 256; i++) {
        bool current_key = W_SYS->keys[i] != 0;
        wagno.keys_pressed[i] = current_key && !wagno.keys[i];
        wagno.keys_released[i] = !current_key && wagno.keys[i];
        wagno.keys[i] = current_key;
    }
    
    // Call user functions
    update();
    
    // Clear drawing state
    wagno.no_fill = false;
    wagno.no_stroke = false;
    wagno.stroke_weight = 1;
    
    // Call user draw
    draw();
    
    // Handle user callbacks
    if (wagno.mouse_pressed) mouse_pressed();
    if (wagno.mouse_released) mouse_released();
    
    for (int i = 0; i < 256; i++) {
        if (wagno.keys_pressed[i]) key_pressed(i);
        if (wagno.keys_released[i]) key_released(i);
    }
    
    w_redraw();
}

#endif // WAGNO_IMPLEMENTATION

#endif // WAGNO_H