# Wagnostic

Minimalist, platform-agnostic WASM runtime for multimedia apps.

## Quick Start

```bash
make -C examples           # builds the wasm3 host and the example ROMs
./examples/wagnostic roms/buttons_test.wasm
```

A ROM allocates a state struct in its linear memory and returns its address from `wupdate()`:

```c
#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

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
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[552];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

int wupdate() {
    rom.s.width = 320;
    rom.s.height = 240;
    rom.s.r_bits = 5; rom.s.r_shift = 11;
    rom.s.g_bits = 6; rom.s.g_shift = 5;
    rom.s.b_bits = 5; rom.s.b_shift = 0;
    rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);

    uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
    vram[rom.s.mouse_y * rom.s.width + rom.s.mouse_x] = 0xF800;

    static struct { uint32_t count; Rect rects[32]; } my_dirty;
    my_dirty.count = 1;
    my_dirty.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty;
    return (int)&rom.s;  // return state pointer; 0 to quit
}
```

## 1. Overview

A Wagnostic ROM is a WebAssembly binary that exports a single function:

- `wupdate()` — called once per frame, **returns a pointer** (`i32` offset into WASM linear memory) to a `WagnosticState` struct.

The Host reads/writes that struct directly. No fixed memory offsets — the ROM decides where the state lives. Large buffers (VRAM, audio) are placed immediately after the struct and referenced via `vram_offset` / `audio_buffer_offset`.

## 2. ROM Requirements

### Must export
- `wupdate()` — called once per frame, returns 0 to quit, or a non-zero pointer to the state struct

### State struct
All other fields are optional. The host applies sensible defaults so a ROM with just `wupdate()` runs immediately:

| Field | Offset | Default | Notes |
|-------|--------|---------|-------|
| `width` | 0 | 320 | window width in pixels |
| `height` | 4 | 240 | window height in pixels |
| `scale` | 8 | 1 | window scale factor |
| `title` | 12 | "Untitled" | window title (char[128]) |
| `dirty_rects` | 140 | 0 | Pointer to {uint32 count; Rect rects[32];} |
| `mouse_x` | 144 | 0 | Mouse X position (int32) |
| `mouse_y` | 148 | 0 | Mouse Y position (int32) |
| `mouse_buttons` | 152 | 0 | Mouse buttons (bit 0=L, bit 1=R) |
| `mouse_wheel` | 156 | 0 | Mouse wheel delta (int32) |
| `keys` | 160 | all 0 | Keyboard state (uint8[256], USB HID) |
| `gamepad_buttons` | 416 | 0 | Gamepad button state |
| `ticks` | 420 | 0 | Time in milliseconds |
| `target_fps` | 424 | 0 | Target FPS (0 = no limit) |
| `vram_offset` | 428 | 0 | Offset from state base to VRAM buffer |
| `r_bits` | 432 | 0 | Red bit count |
| `r_shift` | 436 | 0 | Red bit shift |
| `g_bits` | 440 | 0 | Green bit count |
| `g_shift` | 444 | 0 | Green bit shift |
| `b_bits` | 448 | 0 | Blue bit count |
| `b_shift` | 452 | 0 | Blue bit shift |
| `a_bits` | 456 | 0 | Alpha bit count |
| `a_shift` | 460 | 0 | Alpha bit shift |
| `x_bits` | 464 | 0 | Padding bit count |
| `x_shift` | 468 | 0 | Padding bit shift |
| `unique` | 472 | 0 | Unique session ID |generated when session is created |

**Total struct size: 1024 bytes.**

The minimal viable ROM:

```c
int wupdate() { return 0; }  // quits immediately
```

To open a window and draw:

```c
int wupdate() {
    static State s = {0};
    static uint8_t vram[320 * 240 * 2];
    if (s.width == 0) {
        s.width = 320; s.height = 240;
        s.r_bits = 5; s.r_shift = 11; s.g_bits = 6; s.g_shift = 5; s.b_bits = 5; s.b_shift = 0;
        s.vram_offset = sizeof(State);
    }
    // draw into vram ...
    static struct { uint32_t count; Rect rects[32]; } my_dirty;
    my_dirty.count = 1;
    my_dirty.rects[0] = (Rect){0, 0, 320, 240};
    s.dirty_rects = (uint32_t)&my_dirty;
    return (int)&s;
}
```

## 3. State Struct Fields

### Screen Configuration (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `width` | `uint32` | Screen width in pixels |
| `height` | `uint32` | Screen height in pixels |
| `scale` | `uint32` | Window scale factor |
| `title` | `char[128]` | Window title |

### VRAM (ROM writes, Host reads)
The VRAM buffer is **not** inside the state struct. The ROM allocates it separately and sets `vram_offset` to its distance from the state base:

```c
static struct { State s; uint8_t vram[320 * 240 * 4]; } rom;
rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);
```

The host reads VRAM at `(uint8_t*)state + vram_offset`.

### Dirty Rectangles (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `dirty_rects` | `uint32` | Pointer (WASM offset) to a `{ uint32 count; Rect rects[32]; }` struct |

**Rect struct:** `{ int x, y, w, h; }` — 16 bytes each

### Input (Host writes, ROM reads)
| Name | Type | Description |
|------|------|-------------|
| `mouse_x` | `int32` | Mouse X position |
| `mouse_y` | `int32` | Mouse Y position |
| `mouse_buttons` | `uint32` | Mouse buttons (bit 0=L, bit 1=R) |
| `mouse_wheel` | `int32` | Mouse wheel delta |
| `keys` | `uint8[256]` | Keyboard state (USB HID scancodes) |
| `gamepad_buttons` | `uint32` | Gamepad button state |

### Timing (Host writes, ROM reads)
| Name | Type | Description |
|------|------|-------------|
| `ticks` | `uint32` | Time in milliseconds |

### Framerate Control (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `target_fps` | `uint32` | Target FPS (0 = no limit, default) |

## 4. Dirty Rectangles

Instead of redrawing the entire screen every frame, the ROM specifies which regions changed.

### How it works

1. ROM allocates a `{ uint32_t count; Rect rects[32]; }` struct
2. ROM draws to its VRAM buffer
3. ROM sets `count` and fills `rects` in the struct
4. ROM sets `state.dirty_rects` to the pointer (address) of that struct
5. Host reads the pointer and renders only the dirty regions

### Values

- `state.dirty_rects = 0` or `count = 0` → nothing changed, host skips rendering
- `count = N` (1-32) → host renders N dirty rectangles
- Set `count = 1` and `rects[0]` for full-screen redraw

### Example

```c
static struct { uint32_t count; Rect rects[32]; } my_dirty_list;

int wupdate() {
    // Move player
    player_x += 1;

    // Mark old and new positions as dirty
    my_dirty_list.count = 2;
    my_dirty_list.rects[0] = (Rect){old_x, player_y, 20, 24};
    my_dirty_list.rects[1] = (Rect){player_x, player_y, 20, 24};
    state.dirty_rects = (uint32_t)&my_dirty_list;
    return (int)&state;
}
```

## 5. Video Formats

Wagnostic uses **Dynamic Bitfield Pixel Formats**.
The bit depths can be any valid power of 2: **1, 2, 4, 8, 16, 24, 32, 64**.
Instead of predefined formats (like RGB565) or indexed color palettes, the Host decodes pixels using the `r_bits`/`r_shift`, `g_bits`/`g_shift`, `b_bits`/`b_shift`, and `a_bits`/`a_shift` fields.
The fields indicate how many bits each channel occupies and how far left they are shifted in the pixel integer.
If `r_bits`, `g_bits`, and `b_bits` are all `0`, but `a_bits > 0`, the format is treated as **Grayscale / Luminance**, where the Alpha channel is replicated into R, G, and B.

## 7. Host Behavior

The Host:
1. Calls `wupdate()` once to get the initial state pointer
2. Each frame: writes input into the state struct
3. Calls `wupdate()`, exits if return value is 0
4. Reads config from the state struct and detects changes
5. Reads `dirty_count` and renders dirty regions
6. Resets `mouse_wheel` to 0
7. If `target_fps > 0`, the host adjusts the frame loop to call `wupdate()` at the target rate (using `SDL_Delay` or `setTimeout`). Setting `target_fps = 0` removes the limit.

## 8. Language Agnosticism

Because the ABI uses a single struct pointer instead of named globals, **any language that compiles to WebAssembly** can be used:

| Language | Return state pointer |
|----------|---------------------|
| **C** | `return (int)&state;` |
| **Rust** | `return &state as *const _ as i32;` |
| **Go (TinyGo)** | `return int(uintptr(unsafe.Pointer(&state)))` |

The only requirement is that `wupdate()` returns the offset of a 1024-byte struct matching the layout above.
