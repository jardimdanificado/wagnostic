# Wagnostic ABI

## Globals-Based API
ROMs export named globals. The Host finds them by name and reads/writes their values.

## Minimal ROM

`wupdate()` is the only required export. The host provides defaults for
everything else so a ROM with just `wupdate()` runs immediately:

- `w_width = 320`, `w_height = 240`
- `w_bpp = 32`, `w_scale = 1`
- `w_title = "Untitled"`
- Audio: off (declared by setting `w_audio_size > 0` plus the rest)
- Input/timing: no-op (declared by exporting the corresponding globals)

```c
int wupdate() { return 1; }
```

## Core Globals

### Screen
- `w_width`, `w_height`, `w_bpp`, `w_scale` (uint32)
- `w_title` (char[128])

### VRAM
- `w_vram` (uint8[]) — pixel buffer

### Dirty Rectangles
- `w_dirty_count` (uint32) — 0=nothing, N=render N rects
- `w_dirty_rects` (Rect[32]) — `{ int x, y, w, h; }`

### Input
- `w_mouse_x`, `w_mouse_y` (int32)
- `w_mouse_buttons` (uint32)
- `w_mouse_wheel` (int32)
- `w_keys` (uint8[256])
- `w_gamepad_buttons` (uint32)

### Timing
- `w_ticks` (uint32)

### Audio
- `w_audio_size`, `w_audio_sample_rate`, `w_audio_bpp`, `w_audio_channels` (uint32)
- `w_audio_write`, `w_audio_read` (uint32)
- `w_audio_buffer` (uint8[])
- `w_audio_underrun`, `w_audio_overrun` (uint32) — diagnostic counters

## Functions

| Export | Signature | Description |
|--------|-----------|-------------|
| `wupdate` | `int wupdate()` | Called each frame. Return 0 to quit. |

## Audio Ring Buffer

The audio system uses a ring buffer shared between the ROM (producer) and
the Host audio callback (consumer). Proper usage requires strict ownership
rules to avoid race conditions.

### Buffer Layout

The ring buffer has `w_audio_size` bytes. Only `size - 1` bytes are usable:
- When `w_audio_write == w_audio_read` → buffer is **EMPTY**
- When `(w_audio_write + 1) % size == w_audio_read` → buffer is **FULL**

This avoids the ambiguity of `w == r` meaning both empty and full.

### Ownership Rules

The Host serializes the audio callback and the ROM's `fill_audio` with a
mutex, so these rules are about avoiding stale-pointer bugs, not memory
races. The `fill_audio()` default in `wagn0.h` follows all of them.

**ROM (producer) — owns `w_audio_write`:**
1. Read `w_audio_read` to compute free space in the ring
2. Write samples into `w_audio_buffer`
3. Update `w_audio_write` ONCE at the very end
4. Don't re-read `w_audio_write` after you start writing (you already
   have the local copy — a re-read would see a stale value)

**Host (consumer) — owns `w_audio_read`:**
1. Read `w_audio_write` ONCE at the start of the callback
2. Process all available samples
3. Write `w_audio_read` ONCE at the very end
4. Never write to `w_audio_write` (ROM owns it)

### Sample Formats (`w_audio_bpp`)

| Value | Format | Bytes | Encoding |
|-------|--------|-------|----------|
| 1 | Unsigned 8-bit PCM | 1 | 128 = silence |
| 2 | Signed 16-bit PCM | 2 | Little-endian, 0 = silence |
| 4 | 32-bit float | 4 | IEEE 754, 0.0 = silence |

### Wrap-Around

The ring buffer is a flat array. When `w_audio_write` is near the end and
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

- `w_audio_underrun` — incremented by Host when buffer is empty and more
  samples were requested. Indicates the ROM is not producing audio fast enough.
- `w_audio_overrun` — incremented when the ROM writes past the readable area.
  Indicates the Host is not consuming audio fast enough.

These are informational. ROMs can read them for debugging but should not
depend on them for logic.

## Examples

### C (no helpers — self-contained)
```c
#include <stdint.h>
uint32_t w_width  = 320;
uint32_t w_height = 240;
uint32_t w_bpp    = 16;
uint8_t  w_vram[320 * 240 * 2];

int wupdate() {
    uint16_t* vram = (uint16_t*)w_vram;
    vram[w_mouse_y * w_width + w_mouse_x] = 0xF800;
    return 1;
}
```

### Rust
```rust
#[no_mangle] pub static mut w_width: u32 = 320;
#[no_mangle] pub static mut w_height: u32 = 240;
#[no_mangle] pub static mut w_bpp: u32 = 16;
#[no_mangle] pub static mut w_vram: [u8; 320*240*2] = [0; 320*240*2];
#[no_mangle] pub static mut w_dirty_count: u32 = 0;

#[no_mangle] pub extern "C" fn wupdate() -> i32 {
    unsafe { w_dirty_count = 1; }
    1
}
```

## Compilation
```bash
clang --target=wasm32 -nostdlib -O3 \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    -Wl,--initial-memory=8388608 main.c -o rom.wasm
```
