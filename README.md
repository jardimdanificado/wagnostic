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
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
    uint32_t dirty_rects;
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

The Host reads/writes that struct directly. No fixed memory offsets — the ROM decides where the state lives. Large buffers (VRAM) are placed immediately after the struct and referenced via `vram_offset`.

## 2. ROM Requirements

### Must export
- `wupdate()` — called once per frame, returns 0 to quit, or a non-zero pointer to the state struct

### State struct
The host applies sensible defaults so a ROM with just `wupdate()` runs immediately:

| Field | Offset | Default | Notes |
|-------|--------|---------|-------|
| `width` | 0 | 320 | window width in pixels |
| `height` | 4 | 240 | window height in pixels |
| `r_bits` | 8 | 0 | Red bit count |
| `r_shift` | 12 | 0 | Red bit shift |
| `g_bits` | 16 | 0 | Green bit count |
| `g_shift` | 20 | 0 | Green bit shift |
| `b_bits` | 24 | 0 | Blue bit count |
| `b_shift` | 28 | 0 | Blue bit shift |
| `a_bits` | 32 | 0 | Alpha bit count |
| `a_shift` | 36 | 0 | Alpha bit shift |
| `vram_offset` | 40 | 0 | Offset from state base to VRAM buffer |
| `dirty_rects` | 44 | 0 | Pointer to {uint32 count; Rect rects[32];} |

**Total struct size: 48 bytes.**

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

## 3. Modular Peripherals (`std:*`)

Input peripherals are requested modularly via `wextension("std:*", NULL)`:

```c
extern void* wextension(const char* name, void* ptr);

static uint8_t* keys = NULL;

int wupdate() {
    if (!keys) {
        keys = (uint8_t*)wextension("std:keyboard", NULL);
    }
    if (keys && keys[0x1D]) {
        // Z key pressed
    }
    return (int)&s;
}
```

- `std:keyboard`: Returns `uint8_t*` (256-byte HID scancodes)
- `std:mouse`: Returns pointer to `{ int32_t x, y; uint32_t buttons; int32_t wheel; }`
- `std:gamepad`: Returns `uint32_t*` (digital gamepad button mask)

## 4. Host Behavior

The Host:
1. Calls `wupdate()` once to get the initial state pointer
2. Calls `wupdate()` each frame, exits if return value is 0
3. Reads config from the state struct and renders dirty regions
4. Updates mapped peripheral buffers directly in WASM memory

## 5. Language Agnosticism

Because the ABI uses a single struct pointer instead of named globals, **any language that compiles to WebAssembly** can be used:

| Language | Return state pointer |
|----------|---------------------|
| **C** | `return (int)&state;` |
| **Rust** | `return &state as *const _ as i32;` |
| **Go (TinyGo)** | `return int(uintptr(unsafe.Pointer(&state)))` |

The only requirement is that `wupdate()` returns the offset of a 48-byte struct matching the layout above.
