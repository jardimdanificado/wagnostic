# WagnO API - Easy Game Development for Wagnostic

WagnO is a high-level API for creating games and interactive applications for the Wagnostic WASM runtime. Inspired by p5.js and LÖVE2D, it provides a simple and intuitive way to create games without dealing with low-level memory management.

## Quick Start

```c
#define WAGNO_IMPLEMENTATION
#include "wagno.h"

void setup() {
    // Initialize your game here
}

void update() {
    // Update game logic here
}

void draw() {
    // Draw your game here
}
```

## API Reference

### Core Types

```c
typedef struct { float x, y; } Vec2;
typedef struct { uint8_t r, g, b, a; } Color;
typedef struct { int x, y, w, h; } Rect;
```

### Global State

```c
// Screen
wagno.width, wagno.height, wagno.bpp, wagno.scale

// Time
wagno.delta_time, wagno.frame_count, wagno.fps

// Mouse
wagno.mouse          // Current position (Vec2)
wagno.pmouse         // Previous position (Vec2)
wagno.mouse_pressed  // True when mouse just pressed
wagno.mouse_released // True when mouse just released
wagno.mouse_down     // True while mouse is held
wagno.mouse_button   // Which button (1=left)

// Keyboard
wagno.keys[256]           // Current state
wagno.keys_pressed[256]   // True when key just pressed
wagno.keys_released[256]  // True when key just released
```

### Color Functions

```c
Color wagno_color_rgb(uint8_t r, uint8_t g, uint8_t b);
Color wagno_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
Color wagno_color_hex(uint32_t hex);  // 0xRRGGBB

// Predefined colors
WAGNO_BLACK, WAGNO_WHITE, WAGNO_RED, WAGNO_GREEN, WAGNO_BLUE
WAGNO_YELLOW, WAGNO_CYAN, WAGNO_MAGENTA, WAGNO_GRAY
WAGNO_ORANGE, WAGNO_PURPLE
```

### Drawing State

```c
wagno_fill(color);        // Set fill color
wagno_no_fill();          // Disable fill
wagno_stroke(color);      // Set stroke color
wagno_no_stroke();        // Disable stroke
wagno_stroke_weight(w);   // Set stroke width
```

### Drawing Primitives

```c
// Background
wagno_background(color);

// Shapes
wagno_rect(x, y, w, h);
wagno_ellipse(x, y, w, h);
wagno_line(x1, y1, x2, y2);
wagno_point(x, y);
wagno_triangle(x1, y1, x2, y2, x3, y3);
wagno_quad(x1, y1, x2, y2, x3, y3, x4, y4);
wagno_arc(x, y, w, h, start_angle, stop_angle);

// Text
wagno_text("Hello", x, y);
wagno_text_size(size);
int wagno_text_width("Hello");
```

### Math Utilities

```c
// Constants
WAGNO_PI, WAGNO_TWO_PI, WAGNO_HALF_PI

// Functions
float wagno_map(value, start1, stop1, start2, stop2);
float wagno_constrain(value, min, max);
float wagno_lerp(a, b, t);
float wagno_dist(x1, y1, x2, y2);
float wagno_sqrt(x);
float wagno_abs(x);
float wagno_min(a, b);
float wagno_max(a, b);
float wagno_random(min, max);        // Random float
int wagno_random_int(min, max);      // Random integer
```

### Vector Functions

```c
Vec2 wagno_vec2(x, y);
Vec2 wagno_vec2_add(a, b);
Vec2 wagno_vec2_sub(a, b);
Vec2 wagno_vec2_mul(v, scalar);
float wagno_vec2_len(v);
Vec2 wagno_vec2_normalize(v);
```

### Image Functions

```c
WagnoImage wagno_create_image(width, height, bpp);
void wagno_image(img, x, y);
void wagno_image_scaled(img, x, y, w, h);
void wagno_load_image(img, data, width, height, bpp);
```

### Audio Functions

```c
void wagno_play_tone(freq, duration, volume);
void wagno_play_noise(duration, volume);
```

## User Functions

You must implement these functions:

```c
void setup(void);           // Called once at start
void update(void);          // Called every frame (before draw)
void draw(void);            // Called every frame
```

Optional callbacks:

```c
void mouse_pressed(void);
void mouse_released(void);
void key_pressed(int key);
void key_released(int key);
```

## Example: Simple Platformer

```c
#define WAGNO_IMPLEMENTATION
#include "wagno.h"

float player_x = 100, player_y = 150;
float velocity_y = 0;
bool on_ground = true;

void setup() {
    wagno.width = 320;
    wagno.height = 240;
    wagno.scale = 4;
}

void update() {
    // Move player with mouse
    if (wagno.mouse_down) {
        if (wagno.mouse.x > player_x) player_x += 3;
        else player_x -= 3;
        
        // Jump if clicking above
        if (wagno.mouse.y < player_y && on_ground) {
            velocity_y = -10;
            on_ground = false;
        }
    }
    
    // Apply gravity
    velocity_y += 0.5;
    player_y += velocity_y;
    
    // Ground collision
    if (player_y > 200) {
        player_y = 200;
        velocity_y = 0;
        on_ground = true;
    }
}

void draw() {
    wagno_background(WAGNO_BLUE);
    
    // Draw ground
    wagno_fill(WAGNO_GREEN);
    wagno_rect(0, 200, 320, 40);
    
    // Draw player
    wagno_fill(WAGNO_RED);
    wagno_rect(player_x, player_y, 20, 24);
    
    // Draw mouse cursor
    wagno_fill(WAGNO_YELLOW);
    wagno_rect(wagno.mouse.x - 2, wagno.mouse.y - 2, 5, 5);
}
```

## Building

```bash
# Build the example
make -C roms/wagno_example

# Run it
./wagnostic wagno_example.wasm
```

## Comparison: Raw Wagnostic vs WagnO

### Raw Wagnostic (low-level)
```c
#include "wagnostic.h"
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

static Olivec_Canvas _oc;
static float player_x = 100;

void winit() {
    w_setup("Game", 320, 240, 16, 4, 0);
    _oc = olivec_canvas(W_FB_PTR, 320, 240, 320, 16);
}

void wupdate() {
    // Manual pixel manipulation
    uint16_t* pixels = (uint16_t*)_oc.pixels;
    for (int i = 0; i < 320 * 240; i++) {
        pixels[i] = W_RGB565(0, 0, 0);  // Clear screen
    }
    
    // Manual input handling
    if (W_SYS->mouse_buttons & 1) {
        player_x += 3;
    }
    
    // Manual rectangle drawing
    for (int y = 150; y < 174; y++) {
        for (int x = (int)player_x; x < (int)player_x + 20; x++) {
            if (x >= 0 && x < 320) {
                pixels[y * 320 + x] = W_RGB565(255, 0, 0);
            }
        }
    }
    
    w_redraw();
}
```

### WagnO (high-level)
```c
#define WAGNO_IMPLEMENTATION
#include "wagno.h"

float player_x = 100;

void setup() {
    wagno.width = 320;
    wagno.height = 240;
    wagno.scale = 4;
}

void update() {
    if (wagno.mouse_down) player_x += 3;
}

void draw() {
    wagno_background(WAGNO_BLACK);
    wagno_fill(WAGNO_RED);
    wagno_rect(player_x, 150, 20, 24);
}
```

## Features

- ✅ Simple, p5.js-inspired API
- ✅ Automatic input handling
- ✅ Built-in math utilities
- ✅ Color constants and conversion
- ✅ Drawing primitives (rect, ellipse, line, etc.)
- ✅ Vector math
- ✅ Image support
- ✅ Audio placeholders
- ✅ No memory management required
- ✅ Works with all Wagnostic runners

## License

Same as Wagnostic - see root LICENSE file.