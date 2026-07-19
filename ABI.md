# Wagnostic ABI

## State-Struct-Pointer API

ROMs export a single function `wupdate()` that returns a pointer (`i32` offset into WASM linear memory) to a `WagnosticState` struct. The Host dereferences that pointer and reads/writes the struct directly.

## Minimal ROM

`wupdate()` is the only required export. Return 0 to quit, or a non-zero pointer to the state struct.

```c
int wupdate() { return 0; }  // quits immediately
```

The host provides defaults for all struct fields, so a ROM with just `wupdate()` that returns a valid pointer runs immediately:

- `width = 320`, `height = 240`
- `bpp = 32`, `scale = 1`
- `title = "Untitled"`
- Audio: off (enabled by setting `audio_size > 0` plus the rest)
- Input/timing: zeroed by the host each frame

## State Struct Layout

The struct is **1024 bytes** total. All offsets are from the base address returned by `wupdate()`.

```c
typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width;           // +0
    uint32_t height;          // +4
    uint32_t bpp;             // +8
    uint32_t scale;           // +12
    char title[128];          // +16
    uint32_t dirty_count;     // +144
    Rect dirty_rects[32];     // +148
    int32_t mouse_x;          // +660
    int32_t mouse_y;          // +664
    uint32_t mouse_buttons;   // +668
    int32_t mouse_wheel;      // +672
    uint8_t keys[256];        // +676
    uint32_t gamepad_buttons; // +932
    uint32_t ticks;           // +936
    uint32_t target_fps;      // +940
    uint32_t audio_size;      // +944
    uint32_t audio_sample_rate; // +948
    uint32_t audio_bpp;       // +952
    uint32_t audio_channels;  // +956
    uint32_t audio_write;     // +960
    uint32_t audio_read;      // +964
    uint32_t audio_underrun;  // +968
    uint32_t audio_overrun;   // +972
    uint32_t vram_offset;     // +976
    uint32_t audio_buffer_offset; // +980
    uint32_t r_bits;          // +984
    uint32_t r_shift;         // +988
    uint32_t g_bits;          // +992
    uint32_t g_shift;         // +996
    uint32_t b_bits;          // +1000
    uint32_t b_shift;         // +1004
    uint32_t a_bits;          // +1008
    uint32_t a_shift;         // +1012
    uint8_t reserved[8];      // +1016
} WagnosticState;  // size = 1024
```

### Screen
- `width`, `height`, `scale` (uint32)
- `bpp` (uint32) - Bits per pixel. Defaults to `32` (RGBA8888) if `0` or omitted.
- `title` (char[128])

### VRAM
VRAM is **not** inside the struct. Allocate it after the struct and set `vram_offset` to its distance from the struct base:

```c
static struct {
    WagnosticState state;
    uint8_t vram[320 * 240 * 2];
} rom;
rom.state.vram_offset = sizeof(WagnosticState);
```

The host reads VRAM at `(uint8_t*)state + vram_offset`.

## 5. Video Formats

Wagnostic uses **Dynamic Bitfield Pixel Formats**.
The bit depths can be any valid power of 2: **1, 2, 4, 8, 16, 24, 32, 64**.
Instead of predefined formats (like RGB565) or indexed color palettes, the Host decodes pixels using the `r_bits`/`r_shift`, `g_bits`/`g_shift`, `b_bits`/`b_shift`, and `a_bits`/`a_shift` fields.
The fields indicate how many bits each channel occupies and how far left they are shifted in the pixel integer.
If `r_bits`, `g_bits`, and `b_bits` are all `0`, but `a_bits > 0`, the format is treated as **Grayscale / Luminance**, where the Alpha channel is replicated into R, G, and B.

### Dirty Rectangles
- `dirty_count` (uint32) — 0=nothing, N=render N rects
- `dirty_rects` (Rect[32]) — `{ int x, y, w, h; }`, 16 bytes each

### Input
- `mouse_x`, `mouse_y` (int32)
- `mouse_buttons` (uint32)
- `mouse_wheel` (int32)
- `keys` (uint8[256])
- `gamepad_buttons` (uint32)

### Timing
- `ticks` (uint32)

### Audio
- `audio_size`, `audio_sample_rate`, `audio_bpp`, `audio_channels` (uint32)
- `audio_write`, `audio_read` (uint32)
- `audio_underrun`, `audio_overrun` (uint32) — diagnostic counters
- `audio_buffer_offset` (uint32) — offset from state base to audio ring buffer

### Screen Configuration (ROM writes, Host reads)
| Name | Type | Description |
|------|------|-------------|
| `width` | `uint32` | Screen width in pixels |
| `height` | `uint32` | Screen height in pixels |
| `bpp` | `uint32` | Bits per pixel (1, 2, 4, 8, 16, 24, 32, or 64) |
| `scale` | `uint32` | Window scale factor |
| `title` | `char[128]` | Window title |

## Functions

| Export | Signature | Description |
|--------|-----------|-------------|
| `wupdate` | `int wupdate()` | Called each frame. Return 0 to quit, or pointer to state struct. |

## Audio Ring Buffer

The audio system uses a ring buffer shared between the ROM (producer) and
the Host audio callback (consumer). Proper usage requires strict ownership
rules to avoid race conditions.

### Buffer Layout

The audio buffer is allocated separately from the state struct:

```c
static struct {
    WagnosticState state;
    uint8_t vram[320 * 240 * 2];
    uint8_t audio_buffer[16384];
} rom;
rom.state.audio_buffer_offset = (uint8_t*)rom.audio_buffer - (uint8_t*)&rom.state;
```

The host reads it at `(uint8_t*)state + audio_buffer_offset`.

The ring buffer has `audio_size` bytes. Only `size - 1` bytes are usable:
- When `audio_write == audio_read` → buffer is **EMPTY**
- When `(audio_write + 1) % size == audio_read` → buffer is **FULL**

This avoids the ambiguity of `w == r` meaning both empty and full.

### Ownership Rules

The Host serializes the audio callback and the ROM's `fill_audio` with a
mutex, so these rules are about avoiding stale-pointer bugs, not memory
races.

**ROM (producer) — owns `audio_write`:**
1. Read `audio_read` to compute free space in the ring
2. Write samples into the audio buffer at `(uint8_t*)state + audio_buffer_offset`
3. Update `audio_write` ONCE at the very end
4. Don't re-read `audio_write` after you start writing (you already
   have the local copy — a re-read would see a stale value)

**Host (consumer) — owns `audio_read`:**
1. Read `audio_write` ONCE at the start of the callback
2. Process all available samples
3. Write `audio_read` ONCE at the very end
4. Never write to `audio_write` (ROM owns it)

### Sample Formats (`audio_bpp`)

| Value | Format | Bytes | Encoding |
|-------|--------|-------|----------|
| 1 | Unsigned 8-bit PCM | 1 | 128 = silence |
| 2 | Signed 16-bit PCM | 2 | Little-endian, 0 = silence |
| 4 | 32-bit float | 4 | IEEE 754, 0.0 = silence |

### Wrap-Around

The ring buffer is a flat array. When `audio_write` is near the end and
a multi-byte sample (s16 = 2 bytes, f32 = 4 bytes) would straddle the
end, the ROM must split the write:

```
Buffer: [... data ... | 1 byte | ← wrap → | 1 byte | ... data ... ]
                              ^pos                    ^(pos+1)%size
```

For an s16 at the boundary: write the low byte at `pos`, then on the
next iteration the high byte wraps to position 0. The Host already
handles the read-side wrap, so it does not care that the bytes arrived
in two separate writes.

### Diagnostic Counters

- `audio_underrun` — incremented by Host when buffer is empty and more
  samples were requested. Indicates the ROM is not producing audio fast enough.
- `audio_overrun` — incremented when the ROM writes past the readable area.
  Indicates the Host is not consuming audio fast enough.

These are informational. ROMs can read them for debugging but should not
depend on them for logic.

## Example

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
    uint32_t r_bits;
    uint32_t r_shift;
    uint32_t g_bits;
    uint32_t g_shift;
    uint32_t b_bits;
    uint32_t b_shift;
    uint32_t a_bits;
    uint32_t a_shift;
    uint8_t reserved[8];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

int wupdate() {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.vram_offset = sizeof(State);
    }
    uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
    vram[rom.s.mouse_y * rom.s.width + rom.s.mouse_x] = 0xF800;
    rom.s.dirty_count = 1;
    rom.s.dirty_rects[0] = (Rect){0, 0, 320, 240};
    return (int)&rom.s;
}
```

## Compilation

```bash
clang --target=wasm32 -nostdlib -O3 \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    main.c -o rom.wasm
```

Only `wupdate` needs to be exported. The `--export-all` flag is used for
convenience; in production you can export only `wupdate` and `memory`.

## Size Guarantee

The struct size is guaranteed to be exactly **1024 bytes** across all
compilers and languages. Hosts include a compile-time assertion:

```c
static_assert(sizeof(WagnosticState) == 1024, "Size mismatch");
```

If a compiler inserts unexpected padding, the build fails immediately.
