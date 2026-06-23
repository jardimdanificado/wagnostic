# Wagnostic

Wagnostic is a minimalist, platform-agnostic specification and runtime for multimedia applications.

## Quick Start

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_setup("Hello Wagnostic", 320, 240, 16, 4, 0);
}

void wupdate() {
    int mx = w_mouse_x;
    int my = w_mouse_y;
    
    uint16_t* vram = (uint16_t*)w_vram;
    vram[my * w_width + mx] = W_RGB565(255, 0, 0);
    
    w_redraw();  // Mark full screen as dirty
}
```

## Building

```bash
make -C examples
./examples/wagnostic examples/buttons_test.wasm
```

---

# Full Specification

## 1. Overview

A Wagnostic ROM is a WebAssembly binary that exports named globals. The Host reads/writes these globals to exchange state. No fixed memory offsets — the Host finds globals by name.

## 2. ROM Requirements

### Must export

- `winit()` — called once after instantiation
- `wupdate()` — called once per frame

### Must define globals

See Section 3 for the full list of globals.

## 3. Global Variables

### Control (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_running` | `uint32` | 1=running, 0=quit |

### Screen Configuration (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_width` | `uint32` | Screen width in pixels |
| `w_height` | `uint32` | Screen height in pixels |
| `w_bpp` | `uint32` | Bits per pixel (8, 16, or 32) |
| `w_scale` | `uint32` | Window scale factor |
| `w_title` | `char[128]` | Window title |

### VRAM (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_vram` | `uint8[]` | Pixel buffer |

### Dirty Rectangles (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_dirty_count` | `uint32` | 0=nothing, N=render N rects |
| `w_dirty_rects` | `Rect[32]` | Array of dirty rectangles |

**Rect struct:** `{ int x, y, w, h; }` — 16 bytes each

### Input (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_mouse_x` | `int32` | Mouse X position |
| `w_mouse_y` | `int32` | Mouse Y position |
| `w_mouse_buttons` | `uint32` | Mouse buttons (bit 0=L, bit 1=R) |
| `w_mouse_wheel` | `int32` | Mouse wheel delta |
| `w_keys` | `uint8[256]` | Keyboard state (USB HID scancodes) |
| `w_gamepad_buttons` | `uint32` | Gamepad button state |

### Timing (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_ticks` | `uint32` | Time in milliseconds |

### Audio (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_audio_size` | `uint32` | Buffer size |
| `w_audio_sample_rate` | `uint32` | Sample rate (Hz) |
| `w_audio_bpp` | `uint32` | Bytes per sample (1, 2, or 4) |
| `w_audio_channels` | `uint32` | Number of channels |
| `w_audio_write` | `uint32` | Write position |
| `w_audio_read` | `uint32` | Read position |
| `w_audio_buffer` | `uint8[]` | Audio ring buffer |

## 4. Dirty Rectangles

Instead of redrawing the entire screen every frame, the ROM specifies which regions changed.

### How it works

1. ROM draws to `w_vram`
2. ROM sets `w_dirty_count` and fills `w_dirty_rects`
3. Host renders only the dirty regions

### Values

- `w_dirty_count = 0` → nothing changed, host skips rendering
- `w_dirty_count = N` (1-32) → host renders N dirty rectangles
- Use `w_redraw()` to mark full screen as dirty
- Use `w_redraw_rect(x, y, w, h)` to add a dirty rectangle

### Example

```c
void wupdate() {
    // Move player
    player_x += 1;
    
    // Mark old and new positions as dirty
    w_dirty_count = 2;
    w_dirty_rects[0] = (Rect){old_x, player_y, 20, 24};
    w_dirty_rects[1] = (Rect){player_x, player_y, 20, 24};
}
```

## 5. Video Formats

### 8-bit: RGB332
```c
pixel = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));
```

### 16-bit: RGB565
```c
pixel = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
```

### 32-bit: RGBA8888
```c
pixel = (a << 24) | (b << 16) | (g << 8) | r;
```

## 6. Audio

Ring buffer at `w_audio_buffer`. Format determined by `w_audio_bpp`:
- 1: Unsigned 8-bit PCM
- 2: Signed 16-bit PCM
- 4: 32-bit float

## 7. Host Behavior

The Host:
1. Writes input to globals
2. Calls `wupdate()`
3. Checks `w_running` — exits if 0
4. Auto-detects config changes (resizes window if needed)
5. Reads `w_dirty_count` and renders dirty regions
6. Resets `w_mouse_wheel` to 0