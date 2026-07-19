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

#pragma pack(push, 1)
typedef struct {
    uint32_t width;           // +0
    uint32_t height;          // +4
    uint32_t scale;           // +8
    char title[128];          // +12
    uint32_t dirty_rects;     // +140
    int32_t mouse_x;          // +144
    int32_t mouse_y;          // +148
    uint32_t mouse_buttons;   // +152
    int32_t mouse_wheel;      // +156
    uint8_t keys[256];        // +160
    uint32_t gamepad_buttons; // +416
    uint32_t ticks;           // +420
    uint32_t target_fps;      // +424
    uint32_t audio_size;      // +428
    uint32_t audio_sample_rate; // +432
    uint32_t audio_bpp;       // +436
    uint32_t audio_channels;  // +440
    uint32_t audio_write;     // +444
    uint32_t audio_read;      // +448
    uint32_t audio_underrun;  // +452
    uint32_t audio_overrun;   // +456
    uint32_t vram_offset;     // +460
    uint32_t audio_buffer_offset; // +464
    uint32_t r_bits;          // +468
    uint32_t r_shift;         // +472
    uint32_t g_bits;          // +476
    uint32_t g_shift;         // +480
    uint32_t b_bits;          // +484
    uint32_t b_shift;         // +488
    uint32_t a_bits;          // +492
    uint32_t a_shift;         // +496
    uint32_t x_bits;          // +500
    uint32_t x_shift;         // +504
    uint8_t is_signed;        // +508
    uint8_t is_float;         // +509
    uint8_t is_shared_exponent; // +510
    uint8_t format_padding;   // +511
    uint8_t reserved[512];    // +512
} WagnosticState;  // size = 1024
#pragma pack(pop)
```

### Screen
- `width`, `height`, `scale` (uint32)
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
Instead of predefined formats (like RGB565) or indexed color palettes, the Host decodes pixels using the `r_bits`/`r_shift`, `g_bits`/`g_shift`, `b_bits`/`b_shift`, `a_bits`/`a_shift`, and `x_bits`/`x_shift` fields.

Wagnostic computes the pixel size (stride) automatically in real-time by finding the `MAX(shift + bits)` of all 5 channels. The developer has total freedom to specify overlapping bits, empty padding bits (`x`), and the engine handles reading the exact memory footprint naturally without assumptions.

Additionally, three flags control the data type interpretation of these bits:
- `is_signed` (uint8): If `1`, the bits are interpreted as two's complement signed normalized integers (e.g., SNORM).
- `is_float` (uint8): If `1`, the bits are decoded as IEEE 754 floating point numbers (16-bit half, 32-bit single, or 64-bit double).
- `is_shared_exponent` (uint8): If `1`, the alpha channel bits (`a_bits`/`a_shift`) are treated as a shared exponent applied to the RGB mantissas (e.g., RGB9E5).
If `r_bits`, `g_bits`, and `b_bits` are all `0`, but `a_bits > 0`, the format is treated as **Grayscale / Luminance**, where the Alpha channel is replicated into R, G, and B.

### Dirty Rectangles
- `dirty_rects` (uint32) — Pointer offset (relative to base) to an external array of Rects.
  - If `dirty_rects == 0`, the host will perform a **Full Redraw** of the entire screen.
  - If `dirty_rects > 0`, the host reads `(uint32_t)count` at that offset, followed by `count` structs of `Rect { int32_t x, y, w, h }` (16 bytes each).

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
#pragma pack(push, 1)
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
    uint32_t audio_size, audio_sample_rate, audio_bpp, audio_channels;
    uint32_t audio_write, audio_read;
    uint32_t audio_underrun, audio_overrun;
    uint32_t vram_offset;
    uint32_t audio_buffer_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    uint8_t is_signed, is_float, is_shared_exponent, format_padding;
    uint8_t reserved[512];
} State;
#pragma pack(pop)

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

int wupdate() {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.r_bits = 5; rom.s.r_shift = 11;
        rom.s.g_bits = 6; rom.s.g_shift = 5;
        rom.s.b_bits = 5; rom.s.b_shift = 0;
        rom.s.vram_offset = sizeof(State);
    }
    uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
    vram[rom.s.mouse_y * rom.s.width + rom.s.mouse_x] = 0xF800;
    rom.s.dirty_rects = 0; // 0 = Full redraw
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
