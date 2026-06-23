# Wagnostic Examples

This directory contains everything needed to build and run Wagnostic ROMs, including helper libraries, tools, and example runners.

## Structure

```
examples/
├── Makefile          # Build all ROMs and the native host
├── README.md         # This file
├── BENCHMARK.md      # Benchmark results
├── include/          # Helper headers for ROM development
│   ├── wagnostic.h   # Main API (globals-based)
│   └── olive.h       # Drawing library
├── roms/             # Example ROM source code
│   ├── audio_example/    # Audio playback example
│   ├── draw_example/     # Drawing primitives example
│   ├── images_example/   # Image loading example
│   ├── roguelike_example/ # Roguelike game example
│   ├── tracker_example/  # Music tracker example
│   ├── mouse_platformer/ # Platformer with mouse control
│   ├── wagno_example/    # High-level WagnO API example
│   └── *.c               # Simple test ROMs
├── runners/          # Host implementations
│   ├── native/       # wasm3 interpreter host
│   ├── native-spidermonkey/ # SpiderMonkey C++ host
│   └── web/          # Browser-based host
└── tools/            # Build utilities
    ├── img2wasm.sh   # Convert images to C headers
    └── audio2pcm.py  # Convert audio to PCM data
```

## Building

```bash
# Build native host and all ROMs
make -C examples

# Build only the native host
make -C examples host

# Build only ROMs
make -C examples roms

# Build audio examples (requires ffmpeg)
make -C examples audio
```

## Helper Libraries

### wagnostic.h

Main API header for ROM development. Uses named globals instead of fixed memory offsets.

**Key components:**

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

// Screen configuration (ROM writes, Host reads)
uint32_t w_width;      // Screen width
uint32_t w_height;     // Screen height
uint32_t w_bpp;        // Bits per pixel (8, 16, 32)
uint32_t w_scale;      // Window scale
char w_title[128];     // Window title

// VRAM (ROM writes, Host reads)
uint8_t w_vram[];      // Pixel buffer

// Input (Host writes, ROM reads)
int32_t w_mouse_x;     // Mouse X
int32_t w_mouse_y;     // Mouse Y
uint32_t w_mouse_buttons; // Mouse buttons
int32_t w_mouse_wheel; // Mouse wheel
uint8_t w_keys[256];   // Keyboard state
uint32_t w_gamepad_buttons; // Gamepad buttons

// Timing (Host writes, ROM reads)
uint32_t w_ticks;      // Time in ms

// Audio (ROM writes, Host reads)
uint32_t w_audio_size;
uint32_t w_audio_sample_rate;
uint32_t w_audio_bpp;
uint32_t w_audio_channels;
uint32_t w_audio_write;
uint32_t w_audio_read;
uint8_t w_audio_buffer[];

// Signals (ROM writes, Host reads)
uint8_t w_signal_redraw;
uint8_t w_signal_quit;
uint8_t w_signal_update_window;
uint8_t w_signal_update_audio;

// Helper functions
void w_setup(const char* title, int w, int h, int bpp, int scale, int unused);
void w_redraw();
void* w_audio_ptr();

// Color macros
#define W_RGB565(r, g, b)
#define W_RGBA(r, g, b, a)
#define W_RGB332(r, g, b)

// Gamepad constants
#define W_BTN_UP, W_BTN_DOWN, W_BTN_LEFT, W_BTN_RIGHT
#define W_BTN_A, W_BTN_B, W_BTN_X, W_BTN_Y
#define W_BTN_L1, W_BTN_R1, W_BTN_START, W_BTN_SELECT
```

### olive.h

A single-header drawing library adapted from olive.c. Provides 2D primitives for all supported pixel formats (8/16/32 bpp).

**Usage:**
```c
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

// Create canvas from framebuffer
Olivec_Canvas oc = olivec_canvas(w_vram, w_width, w_height, w_width, w_bpp);

// Draw primitives
olivec_fill(oc, W_RGB565(0, 0, 0));
olivec_rect(oc, 10, 10, 50, 30, W_RGB565(255, 0, 0));
olivec_circle(oc, 100, 100, 25, W_RGB565(0, 255, 0));
olivec_line(oc, 0, 0, 320, 240, W_RGB565(0, 0, 255));
olivec_text(oc, "Hello!", 10, 10, olivec_default_font, 1, W_RGB565(255, 255, 255));
```

## Tools

### img2wasm.sh

Converts any image to a C header with raw pixel data.

```bash
./tools/img2wasm.sh image.webp --rgb565   # → image.rgb565.h
./tools/img2wasm.sh photo.jpg --rgba      # → photo.rgba.h
```

### audio2pcm.py

Converts audio files to PCM data as C header files.

```bash
./tools/audio2pcm.py music.mp3 audio_data.h --name music
```

## Creating a ROM

### Minimal ROM example

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_setup("My ROM", 320, 240, 16, 2, 0);
}

void wupdate() {
    uint16_t* vram = (uint16_t*)w_vram;
    
    // Clear screen
    for (int i = 0; i < w_width * w_height; i++) {
        vram[i] = W_RGB565(0, 0, 128);
    }
    
    // Draw based on input
    if (w_gamepad_buttons & W_BTN_A) {
        vram[100 * w_width + 100] = W_RGB565(255, 255, 0);
    }
    
    w_redraw();
}
```

### Compilation flags

```bash
clang --target=wasm32 \
    -nostdlib \
    -fno-delete-null-pointer-checks \
    -O3 \
    -Iinclude \
    -Wl,--no-entry \
    -Wl,--export-all \
    -Wl,--allow-undefined \
    -Wl,--initial-memory=8388608 \
    main.c -o output.wasm
```

## Runners

### Native wasm3 (SDL2)

```bash
./examples/wagnostic rom.wasm
```

### Native SpiderMonkey (SDL2 + OpenGL)

```bash
./examples/wagnostic-sm rom.wasm
```

### Web (Canvas 2D)

Open `examples/runners/web/index.html`.

## Example ROMs

| ROM | Description |
|-----|-------------|
| `buttons_test.wasm` | Gamepad input test |
| `test_8bpp.wasm` | 8-bit color test |
| `test_16bpp.wasm` | 16-bit color test |
| `test_32bpp.wasm` | 32-bit color test |
| `audio_test.wasm` | Simple audio test |
| `fallback_test.wasm` | Fallback rendering test |
| `terminal_test.wasm` | Terminal-style display |
| `draw_example.wasm` | Drawing primitives demo |
| `images_example.wasm` | Image loading demo |
| `roguelike_example.wasm` | Roguelike game |
| `tracker_example.wasm` | Music tracker |
| `audio_example.wasm` | Full audio demo |
| `mouse_platformer.wasm` | Platformer with mouse control |
| `wagno_example.wasm` | High-level WagnO API example |
| `benchmark_cpu.wasm` | CPU benchmark (Mandelbrot) |
| `benchmark_vram.wasm` | VRAM bandwidth benchmark |
| `benchmark_audio.wasm` | Audio processing benchmark |
| `benchmark_particles.wasm` | Particle system benchmark |
| `benchmark_all.wasm` | Combined benchmark |