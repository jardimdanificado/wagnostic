# Wagnostic Examples

This directory contains everything needed to build and run Wagnostic ROMs, including helper libraries, tools, and example runners.

## Structure

```
examples/
├── Makefile          # Build all ROMs and the native host
├── README.md         # This file
├── include/          # Helper headers for ROM development
│   ├── wagnostic.h   # System helpers and constants
│   └── olive.h       # Drawing library
├── rom/              # Example ROM source code
│   ├── audio/        # Audio playback example
│   ├── draw/         # Drawing primitives example
│   ├── images/       # Image loading example
│   ├── roguelike/    # Roguelike game example
│   ├── tracker/      # Music tracker example
│   └── *.c           # Simple test ROMs
├── runners/          # Host implementations
│   ├── native/       # SDL2 + OpenGL host
│   ├── web/          # Browser-based host
│   └── love/         # LÖVE2D host
└── tools/            # Build utilities
    ├── img2fb.sh     # Convert images to framebuffer format
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

Optional helper header that provides convenient macros and functions for ROM development. A ROM can be written without it by directly manipulating memory.

**Key components:**

```c
// System pointer (mapped at address 0)
#define W_SYS ((volatile Wagnostic_System*)0)

// Framebuffer pointer (VRAM starts at 512)
#define W_FB_PTR ((void*)512)

// Audio buffer pointer (dynamic, based on video config)
static inline void* w_audio_ptr();

// Signal opcodes
#define W_SIG_REDRAW        1
#define W_SIG_QUIT          2
#define W_SIG_UPDATE_TITLE  3
#define W_SIG_UPDATE_WINDOW 4
#define W_SIG_UPDATE_AUDIO  5
#define W_SIG_LOG_INFO      6
#define W_SIG_LOG_WARN      7
#define W_SIG_LOG_ERR       8

// Gamepad buttons
#define W_BTN_UP     (1 << 0)
#define W_BTN_DOWN   (1 << 1)
// ... etc

// Convenience functions
static inline void w_setup(const char* title, int w, int h, int bpp, int scale, int unused);
static inline void w_redraw();

// Color encoding macros
#define W_RGB332(r, g, b)  (uint8_t)(((r) & 0xE0) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))
#define W_RGB565(r, g, b)  (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define W_RGBA(r, g, b, a) (uint32_t)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r))
```

### olive.h

A single-header drawing library adapted from olive.c. Provides 2D primitives for all supported pixel formats (8/16/32 bpp).

**Usage:**
```c
#define OLIVEC_IMPLEMENTATION
#include "olive.h"

// Create canvas from framebuffer
Olivec_Canvas oc = olivec_canvas(W_FB_PTR, W_SYS->width, W_SYS->height, W_SYS->width, W_SYS->bpp);

// Draw primitives
olivec_fill(oc, W_RGB565(0, 0, 0));           // Clear to black
olivec_rect(oc, 10, 10, 50, 30, W_RGB565(255, 0, 0));  // Red rectangle
olivec_circle(oc, 100, 100, 25, W_RGB565(0, 255, 0));  // Green circle
olivec_line(oc, 0, 0, 320, 240, W_RGB565(0, 0, 255)); // Blue line
olivec_text(oc, "Hello!", 10, 10, olivec_default_font, 1, W_RGB565(255, 255, 255));
olivec_triangle(oc, x1, y1, x2, y2, x3, y3, color);
olivec_triangle3uv(oc, ...);  // Textured triangle
olivec_sprite_copy(dst, x, y, w, h, src);
```

## Tools

### img2fb.sh

Converts images to raw framebuffer data and C header files.

```bash
# Usage: img2fb.sh <input_image> <output_name> <bpp>
./tools/img2fb.sh image.webp output_name 16
# Creates: output_name.h and output_name.raw
```

### audio2pcm.py

Converts audio files to PCM data as C header files.

```bash
# Usage: audio2pcm.py <input_audio> <output_header> [--name <name>]
./tools/audio2pcm.py music.mp3 audio_data.h --name music
```

Requires `ffmpeg` to be installed.

## Creating a ROM

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
    -Wl,--global-base=1048576 \
    -Wl,--initial-memory=8388608 \
    main.c -o output.wasm
```

**Key flags:**
- `--target=wasm32`: Compile to WebAssembly
- `-nostdlib`: No standard library (bare metal)
- `-Wl,--no-entry`: No main() required
- `-Wl,--export-all`: Export winit and wupdate
- `-Wl,--global-base=1048576`: Start memory at 1MB (0x100000)
- `-Wl,--initial-memory=8388608`: 8MB initial memory

### Minimal ROM example

```c
#include "wagnostic.h"

void winit() {
    w_setup("My ROM", 320, 240, 16, 2, 0);
}

void wupdate() {
    uint16_t* vram = (uint16_t*)W_FB_PTR;
    
    // Clear screen
    for (int i = 0; i < 320 * 240; i++) {
        vram[i] = W_RGB565(0, 0, 128);
    }
    
    // Draw based on input
    if (W_SYS->gamepad_buttons & W_BTN_A) {
        vram[100 * 320 + 100] = W_RGB565(255, 255, 0);
    }
    
    w_redraw();
}
```

## Runners

### Native wasm3 (SDL2 + OpenGL)

Full-featured desktop runner using the wasm3 interpreter, with audio, gamepad, and mouse support.

```bash
./examples/wagnostic rom.wasm
```

### Native GPU (SDL2 + OpenGL 3.3+ Core)

Wasmtime JIT-powered GPU-accelerated runner with triple buffer + PBO async uploads and GLSL pixel format conversion.

```bash
make -C examples host-gpu
./examples/wagnostic-gpu rom.wasm
```

**Features:**
- Wasmtime Cranelift JIT (3-10x faster than wasm3)
- GPU shader handles RGB332, RGB565, and RGBA8888 conversion
- PBO async texture uploads (no CPU-GPU stalls)
- Triple buffering for maximum throughput
- Nearest-neighbor texture filtering for pixel-perfect scaling

### Web (Canvas 2D)

Simpler browser-based runner using Canvas 2D API.

Open `examples/runners/web/index.html`.

### LÖVE2D

Requires LÖVE2D and the wasm3 shared library.

```bash
love examples/runners/love rom.wasm
```

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
