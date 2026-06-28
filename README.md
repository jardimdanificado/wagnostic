# Wagnostic

Minimalist, platform-agnostic WASM runtime for multimedia apps.

## Quick Start

```bash
make -C examples           # builds the wasm3 host and the example ROMs
./examples/wagnostic examples/roms/buttons_test/buttons_test.wasm
```

A ROM exports globals for the screen, input, and audio, plus a single function:

```c
#include <stdint.h>

uint32_t w_width  = 320;
uint32_t w_height = 240;
uint32_t w_bpp    = 16;
uint8_t  w_vram[320 * 240 * 2];

int wupdate() {
    uint16_t* vram = (uint16_t*)w_vram;
    vram[w_mouse_y * w_width + w_mouse_x] = 0xF800;
    return 1;  // return 0 to quit
}
```

## 1. Overview

A Wagnostic ROM is a WebAssembly binary that exports named globals. The Host reads/writes these globals to exchange state. No fixed memory offsets — the Host finds globals by name.

## 2. ROM Requirements

### Must export
- `wupdate()` — called once per frame, returns 0 to quit

### Optional globals
All other globals are optional. The host applies sensible defaults so a ROM with just `wupdate()` runs immediately:

| Global | Default | Notes |
|--------|---------|-------|
| `w_width` | 320 | window width in pixels |
| `w_height` | 240 | window height in pixels |
| `w_bpp` | 32 | bits per pixel (8/16/32) |
| `w_scale` | 1 | window scale factor |
| `w_title` | "Untitled" | window title |
| `w_vram` | — | not provided: nothing to render |
| `w_dirty_count` / `w_dirty_rects` | — | not provided: nothing to render |
| `w_audio_*` | — | all zero: audio is opt-in |
| `w_mouse_*` / `w_keys` / `w_gamepad_buttons` | — | reads return 0, writes are no-ops |
| `w_ticks` | — | reads return 0 |
| `w_target_fps` | 0 | no framerate limit |

The minimal viable ROM:

```c
int wupdate() { return 1; }
```

opens a 320×240 window titled "Untitled", runs at 32bpp, and exits when
the user closes the window. To draw, declare a `w_vram` buffer and start
writing to it. To use audio, declare `w_audio_size > 0` plus the rest of
the audio globals.

## 3. Global Variables

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

### Framerate Control (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `w_target_fps` | `uint32` | Target FPS (0 = no limit, default) |

### Audio

| Name | Type | Direction | Description |
|------|------|-----------|-------------|
| `w_audio_size` | `uint32` | ROM → Host | Buffer size in bytes |
| `w_audio_sample_rate` | `uint32` | ROM → Host | Sample rate (Hz) |
| `w_audio_bpp` | `uint32` | ROM → Host | Bytes per sample (1=u8, 2=s16, 4=f32) |
| `w_audio_channels` | `uint32` | ROM → Host | Number of channels |
| `w_audio_write` | `uint32` | ROM → Host | Write position |
| `w_audio_read` | `uint32` | Host → ROM | Read position |
| `w_audio_buffer` | `uint8[]` | shared | Ring buffer (16384 bytes) |
| `w_audio_underrun` | `uint32` | Host → ROM | Underrun counter |
| `w_audio_overrun` | `uint32` | Host → ROM | Overrun counter |

`w_audio_bpp` is bytes per sample (not bits per pixel). The host opens
the SDL audio device with the format from these fields and reopens it
when any of them change.

## 4. Dirty Rectangles

Instead of redrawing the entire screen every frame, the ROM specifies which regions changed.

### How it works

1. ROM draws to `w_vram`
2. ROM sets `w_dirty_count` and fills `w_dirty_rects`
3. Host renders only the dirty regions

### Values

- `w_dirty_count = 0` → nothing changed, host skips rendering
- `w_dirty_count = N` (1-32) → host renders N dirty rectangles
- Set `w_dirty_count = 1` and `w_dirty_rects[0]` for full-screen redraw

### Example

```c
int wupdate() {
    // Move player
    player_x += 1;

    // Mark old and new positions as dirty
    w_dirty_count = 2;
    w_dirty_rects[0] = (Rect){old_x, player_y, 20, 24};
    w_dirty_rects[1] = (Rect){player_x, player_y, 20, 24};
    return 1;
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
- 1: Unsigned 8-bit PCM (128 = silence)
- 2: Signed 16-bit PCM (little-endian, 0 = silence)
- 4: 32-bit float (0.0 = silence)

### Ring Buffer Rules

The buffer uses `size - 1` usable bytes:
- `w_audio_write == w_audio_read` → **EMPTY**
- `(w_audio_write + 1) % size == w_audio_read` → **FULL**

The Host protects this buffer with a mutex, so the ownership rules below
are about avoiding stale-pointer bugs, not memory races. The `fill_audio()`
default in `wagn0.h` follows all of them.

**ROM (producer) — owns `w_audio_write`:**
1. Read `w_audio_read` to compute free space
2. Write samples into `w_audio_buffer`
3. Update `w_audio_write` ONCE at the end
4. Don't re-read `w_audio_write` after you start writing (you already have the local copy)

**Host (consumer) — owns `w_audio_read`:**
1. Read `w_audio_write` ONCE at callback start
2. Process samples
3. Write `w_audio_read` ONCE at callback end
4. Never write to `w_audio_write`

See [ABI.md](ABI.md) for the diagnostic counters and wrap-around details.

## 7. Host Behavior

The Host:
1. Writes input to globals
2. Calls `wupdate()`, exits if return value is 0
3. Auto-detects config changes (resizes window if needed)
4. Reads `w_dirty_count` and renders dirty regions
5. Resets `w_mouse_wheel` to 0
6. If `w_target_fps > 0`, the host adjusts the frame loop to call `wupdate()` at the target rate (using `SDL_Delay` or `setTimeout`). Setting `w_target_fps = 0` removes the limit.
