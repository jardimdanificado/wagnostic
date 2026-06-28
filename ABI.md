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

The struct is **984 bytes** total. All offsets are from the base address returned by `wupdate()`.

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
} WagnosticState;  // size = 984
```

### Screen
- `width`, `height`, `bpp`, `scale` (uint32)
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

## Examples

### C (no helpers — self-contained)
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

### AssemblyScript
```ts
const STATE_SIZE = 984;
const VRAM_SIZE = 320 * 240 * 2;

let state_ptr: usize = 0;
let vram_ptr: usize = 0;

function init(): void {
  state_ptr = memory.allocate(STATE_SIZE + VRAM_SIZE);
  vram_ptr = state_ptr + STATE_SIZE;
  store<u32>(state_ptr + 0, 320);   // width
  store<u32>(state_ptr + 4, 240);   // height
  store<u32>(state_ptr + 8, 16);    // bpp
  store<u32>(state_ptr + 976, STATE_SIZE); // vram_offset
}

export function wupdate(): i32 {
  if (state_ptr == 0) init();
  // draw to vram_ptr...
  store<u32>(state_ptr + 144, 1);  // dirty_count
  return <i32>state_ptr;
}
```

### Zig
```zig
const Rect = extern struct { x: i32, y: i32, w: i32, h: i32 };

const State = extern struct {
    width: u32, height: u32, bpp: u32, scale: u32,
    title: [128]u8,
    dirty_count: u32,
    dirty_rects: [32]Rect,
    mouse_x: i32, mouse_y: i32,
    mouse_buttons: u32,
    mouse_wheel: i32,
    keys: [256]u8,
    gamepad_buttons: u32,
    ticks: u32,
    target_fps: u32,
    audio_size: u32, audio_sample_rate: u32, audio_bpp: u32, audio_channels: u32,
    audio_write: u32, audio_read: u32,
    audio_underrun: u32, audio_overrun: u32,
    vram_offset: u32,
    audio_buffer_offset: u32,
};

const Rom = extern struct {
    s: State,
    vram: [320 * 240 * 2]u8,
};

var rom: Rom = .{ .s = .{ .width = 0, .height = 0, .bpp = 0, .scale = 0, /* ... */ }, .vram = .{0} ** (320 * 240 * 2) };

export fn wupdate() i32 {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.bpp = 16;
        rom.s.vram_offset = @sizeOf(State);
    }
    const vram = @ptrFromInt(@intFromPtr(&rom.s) + rom.s.vram_offset);
    // draw...
    rom.s.dirty_count = 1;
    return @intCast(i32, @intFromPtr(&rom.s));
}
```

### Rust
```rust
#[repr(C)]
pub struct Rect { pub x: i32, pub y: i32, pub w: i32, pub h: i32 }

#[repr(C)]
pub struct State {
    pub width: u32, pub height: u32, pub bpp: u32, pub scale: u32,
    pub title: [u8; 128],
    pub dirty_count: u32,
    pub dirty_rects: [Rect; 32],
    pub mouse_x: i32, pub mouse_y: i32,
    pub mouse_buttons: u32,
    pub mouse_wheel: i32,
    pub keys: [u8; 256],
    pub gamepad_buttons: u32,
    pub ticks: u32,
    pub target_fps: u32,
    pub audio_size: u32, pub audio_sample_rate: u32, pub audio_bpp: u32, pub audio_channels: u32,
    pub audio_write: u32, pub audio_read: u32,
    pub audio_underrun: u32, pub audio_overrun: u32,
    pub vram_offset: u32,
    pub audio_buffer_offset: u32,
}

static mut ROM: State = State {
    width: 320, height: 240, bpp: 16, scale: 1,
    title: [0; 128],
    dirty_count: 0,
    dirty_rects: [Rect { x: 0, y: 0, w: 0, h: 0 }; 32],
    mouse_x: 0, mouse_y: 0,
    mouse_buttons: 0,
    mouse_wheel: 0,
    keys: [0; 256],
    gamepad_buttons: 0,
    ticks: 0,
    target_fps: 0,
    audio_size: 0, audio_sample_rate: 0, audio_bpp: 0, audio_channels: 0,
    audio_write: 0, audio_read: 0,
    audio_underrun: 0, audio_overrun: 0,
    vram_offset: 0,
    audio_buffer_offset: 0,
};

#[no_mangle]
pub extern "C" fn wupdate() -> i32 {
    unsafe {
        ROM.dirty_count = 1;
        &ROM as *const _ as i32
    }
}
```

## Compilation

```bash
clang --target=wasm32 -nostdlib -O3 \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    -Wl,--initial-memory=8388608 main.c -o rom.wasm
```

Only `wupdate` needs to be exported. The `--export-all` flag is used for
convenience; in production you can export only `wupdate` and `memory`.

## Size Guarantee

The struct size is guaranteed to be exactly **984 bytes** across all
compilers and languages. Hosts include a compile-time assertion:

```c
static_assert(sizeof(WagnosticState) == 984, "Size mismatch");
```

If a compiler inserts unexpected padding, the build fails immediately.
