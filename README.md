# Wagnostic

Minimalist, platform-agnostic WASM runtime for multimedia apps.

## Quick Start

```bash
make -C examples           # builds the wasm3 host and the example ROMs
./examples/wagnostic examples/buttons_test.wasm
```

A ROM allocates a state struct in its linear memory and returns its address from `wupdate()`:

```c
#include <stdint.h>

typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, bpp, scale;
    char title[128];
    uint32_t dirty_count;
    Rect dirty_rects[32];
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

int wupdate() {
    rom.s.width = 320;
    rom.s.height = 240;
    rom.s.bpp = 16;
    rom.s.vram_offset = (uint32_t)((uint8_t*)rom.vram - (uint8_t*)&rom.s);

    uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
    vram[rom.s.mouse_y * rom.s.width + rom.s.mouse_x] = 0xF800;

    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
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
| `bpp` | 8 | 32 | bits per pixel (8/16/32) |
| `scale` | 12 | 1 | window scale factor |
| `title` | 16 | "Untitled" | window title (char[128]) |
| `dirty_count` | 144 | 0 | 0=nothing, N=render N rects |
| `dirty_rects` | 148 | — | Rect[32], each 16 bytes |
| `mouse_x` | 660 | 0 | Mouse X position (int32) |
| `mouse_y` | 664 | 0 | Mouse Y position (int32) |
| `mouse_buttons` | 668 | 0 | Mouse buttons (bit 0=L, bit 1=R) |
| `mouse_wheel` | 672 | 0 | Mouse wheel delta (int32) |
| `keys` | 676 | all 0 | Keyboard state (uint8[256], USB HID) |
| `gamepad_buttons` | 932 | 0 | Gamepad button state |
| `ticks` | 936 | 0 | Time in milliseconds |
| `target_fps` | 940 | 0 | Target FPS (0 = no limit) |
| `audio_size` | 944 | 0 | Buffer size in bytes (0 = audio off) |
| `audio_sample_rate` | 948 | 0 | Sample rate in Hz |
| `audio_bpp` | 952 | 0 | Bytes per sample (1=u8, 2=s16, 4=f32) |
| `audio_channels` | 956 | 0 | Number of channels |
| `audio_write` | 960 | 0 | Write position (ROM → Host) |
| `audio_read` | 964 | 0 | Read position (Host → ROM) |
| `audio_underrun` | 968 | 0 | Underrun counter (Host → ROM) |
| `audio_overrun` | 972 | 0 | Overrun counter (Host → ROM) |
| `vram_offset` | 976 | 0 | Offset from state base to VRAM buffer |
| `audio_buffer_offset` | 980 | 0 | Offset from state base to audio buffer |

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
        s.width = 320; s.height = 240; s.bpp = 16;
        s.vram_offset = sizeof(State);
    }
    // draw into vram ...
    s.dirty_count = 1;
    s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&s;
}
```

## 3. State Struct Fields

### Screen Configuration (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `width` | `uint32` | Screen width in pixels |
| `height` | `uint32` | Screen height in pixels |
| `bpp` | `uint32` | Bits per pixel (8, 16, or 32) |
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
| `dirty_count` | `uint32` | 0=nothing, N=render N rects |
| `dirty_rects` | `Rect[32]` | Array of dirty rectangles |

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

### Audio

| Name | Type | Direction | Description |
|------|------|-----------|-------------|
| `audio_size` | `uint32` | ROM → Host | Buffer size in bytes |
| `audio_sample_rate` | `uint32` | ROM → Host | Sample rate (Hz) |
| `audio_bpp` | `uint32` | ROM → Host | Bytes per sample (1=u8, 2=s16, 4=f32) |
| `audio_channels` | `uint32` | ROM → Host | Number of channels |
| `audio_write` | `uint32` | ROM → Host | Write position |
| `audio_read` | `uint32` | Host → ROM | Read position |
| `audio_underrun` | `uint32` | Host → ROM | Underrun counter |
| `audio_overrun` | `uint32` | Host → ROM | Overrun counter |
| `audio_buffer_offset` | `uint32` | ROM → Host | Offset from state base to audio ring buffer |

The audio buffer is allocated separately, like VRAM:

```c
static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
    uint8_t audio_buffer[16384];
} rom;
rom.s.audio_buffer_offset = (uint32_t)((uint8_t*)rom.audio_buffer - (uint8_t*)&rom.s);
```

`w_audio_bpp` is bytes per sample (not bits per pixel). The host opens
the SDL audio device with the format from these fields and reopens it
when any of them change.

## 4. Dirty Rectangles

Instead of redrawing the entire screen every frame, the ROM specifies which regions changed.

### How it works

1. ROM draws to its VRAM buffer
2. ROM sets `dirty_count` and fills `dirty_rects`
3. Host renders only the dirty regions

### Values

- `dirty_count = 0` → nothing changed, host skips rendering
- `dirty_count = N` (1-32) → host renders N dirty rectangles
- Set `dirty_count = 1` and `dirty_rects[0]` for full-screen redraw

### Example

```c
int wupdate() {
    // Move player
    player_x += 1;

    // Mark old and new positions as dirty
    state.dirty_count = 2;
    state.dirty_rects[0] = (Rect){old_x, player_y, 20, 24};
    state.dirty_rects[1] = (Rect){player_x, player_y, 20, 24};
    return (int)&state;
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

Ring buffer at `(uint8_t*)state + audio_buffer_offset`. Format determined by `audio_bpp`:
- 1: Unsigned 8-bit PCM (128 = silence)
- 2: Signed 16-bit PCM (little-endian, 0 = silence)
- 4: 32-bit float (0.0 = silence)

### Ring Buffer Rules

The buffer uses `size - 1` usable bytes:
- `audio_write == audio_read` → **EMPTY**
- `(audio_write + 1) % size == audio_read` → **FULL**

The Host protects this buffer with a mutex, so the ownership rules below
are about avoiding stale-pointer bugs, not memory races.

**ROM (producer) — owns `audio_write`:**
1. Read `audio_read` to compute free space
2. Write samples into the audio buffer
3. Update `audio_write` ONCE at the end
4. Don't re-read `audio_write` after you start writing (you already have the local copy)

**Host (consumer) — owns `audio_read`:**
1. Read `audio_write` ONCE at callback start
2. Process samples
3. Write `audio_read` ONCE at callback end
4. Never write to `audio_write`

See [ABI.md](ABI.md) for the diagnostic counters and wrap-around details.

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
| **AssemblyScript** | `return <i32>state_ptr;` |
| **Zig** | `return @intCast(i32, @intFromPtr(&state));` |
| **V** | `return int(&state)` |
| **Rust** | `return &state as *const _ as i32;` |
| **Go (TinyGo)** | `return int(uintptr(unsafe.Pointer(&state)))` |

The only requirement is that `wupdate()` returns the offset of a 1024-byte struct matching the layout above.
