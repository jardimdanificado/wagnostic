# Wagnostic ABI

## Globals-Based API

ROMs export named globals. The Host finds them by name and reads/writes their values.

## Core Globals

### Control
- `w_running` (uint32) — 1=running, 0=quit

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

## Helper Functions

```c
w_setup(title, width, height, bpp, scale, unused);  // Set screen config
w_redraw();                                          // Mark full screen dirty
w_redraw_rect(x, y, w, h);                          // Add dirty rectangle
w_audio_ptr();                                       // Get audio buffer pointer
```

## Examples

### C
```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_setup("Game", 320, 240, 16, 4, 0);
}

void wupdate() {
    uint16_t* vram = (uint16_t*)w_vram;
    vram[w_mouse_y * w_width + w_mouse_x] = W_RGB565(255, 0, 0);
    w_redraw();
}
```

### Rust
```rust
#[no_mangle] pub static mut w_width: u32 = 320;
#[no_mangle] pub static mut w_height: u32 = 240;
#[no_mangle] pub static mut w_vram: [u8; 320*240*2] = [0; 320*240*2];
#[no_mangle] pub static mut w_running: u32 = 1;
#[no_mangle] pub static mut w_dirty_count: u32 = 0;

#[no_mangle] pub extern "C" fn winit() { unsafe { w_width = 320; w_height = 240; } }
#[no_mangle] pub extern "C" fn wupdate() { unsafe { w_dirty_count = 1; } }
```

## Compilation

```bash
clang --target=wasm32 -nostdlib -O3 -Iinclude \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    -Wl,--initial-memory=8388608 main.c -o rom.wasm
```