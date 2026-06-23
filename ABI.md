# Wagnostic ABI (Application Binary Interface)

This document describes the binary interface of Wagnostic for developing ROMs in any language that compiles to WASM.

## How It Works

1. The ROM exports named global variables (`w_width`, `w_height`, `w_vram`, etc.)
2. The Host finds these globals by name using the WASM API
3. Each global contains a pointer (i32) into WASM linear memory
4. The Host reads/writes the actual value by dereferencing this pointer

## Global Variables

### Screen Configuration (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_width` | `uint32` | Screen width in pixels |
| `w_height` | `uint32` | Screen height in pixels |
| `w_bpp` | `uint32` | Bits per pixel (8, 16, or 32) |
| `w_scale` | `uint32` | Window scale factor |
| `w_title` | `char[128]` | Window title (null-terminated) |

### VRAM (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_vram` | `uint8[]` | Pixel buffer (size: `w_width * w_height * w_bpp / 8`) |

### Input (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_mouse_x` | `int32` | Mouse X position |
| `w_mouse_y` | `int32` | Mouse Y position |
| `w_mouse_buttons` | `uint32` | Mouse buttons bitmask (bit 0=L, bit 1=R) |
| `w_mouse_wheel` | `int32` | Mouse wheel delta |
| `w_keys` | `uint8[256]` | Keyboard state (USB HID scancodes) |
| `w_gamepad_buttons` | `uint32` | Gamepad button state |

### Timing (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_ticks` | `uint32` | Time in milliseconds since startup |

### Audio (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_audio_size` | `uint32` | Audio buffer size in bytes |
| `w_audio_sample_rate` | `uint32` | Sample rate (Hz) |
| `w_audio_bpp` | `uint32` | Bytes per sample (1, 2, or 4) |
| `w_audio_channels` | `uint32` | Number of channels |
| `w_audio_write` | `uint32` | Write position (ring buffer) |
| `w_audio_read` | `uint32` | Read position (ring buffer) |
| `w_audio_buffer` | `uint8[]` | Audio ring buffer |

### Signals (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_signal_redraw` | `uint8` | Request screen redraw (1=redraw) |
| `w_signal_quit` | `uint8` | Request quit (1=quit) |
| `w_signal_update_window` | `uint8` | Request window update |
| `w_signal_update_audio` | `uint8` | Request audio reinit |

## Examples

### C

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_setup("My Game", 320, 240, 16, 4, 0);
}

void wupdate() {
    int mx = w_mouse_x;
    int my = w_mouse_y;
    
    uint16_t* vram = (uint16_t*)w_vram;
    vram[my * w_width + mx] = W_RGB565(255, 0, 0);
    
    w_redraw();
}
```

### Rust

```rust
#[no_mangle]
pub static mut w_width: u32 = 320;
#[no_mangle]
pub static mut w_height: u32 = 240;
#[no_mangle]
pub static mut w_bpp: u32 = 16;
#[no_mangle]
pub static mut w_scale: u32 = 4;
#[no_mangle]
pub static mut w_vram: [u8; 320 * 240 * 2] = [0; 320 * 240 * 2];
#[no_mangle]
pub static mut w_mouse_x: i32 = 0;
#[no_mangle]
pub static mut w_mouse_y: i32 = 0;
#[no_mangle]
pub static mut w_signal_redraw: u8 = 0;

#[no_mangle]
pub extern "C" fn winit() {
    unsafe {
        w_width = 320;
        w_height = 240;
    }
}

#[no_mangle]
pub extern "C" fn wupdate() {
    unsafe {
        let mx = w_mouse_x as usize;
        let my = w_mouse_y as usize;
        let w = w_width as usize;
        
        let offset = (my * w + mx) * 2;
        w_vram[offset] = 0x00;
        w_vram[offset + 1] = 0xF8;
        
        w_signal_redraw = 1;
    }
}
```

### Go (TinyGo)

```go
package main

//go:wasm_module wagnostic
var (
    w_width         uint32 = 320
    w_height        uint32 = 240
    w_bpp           uint32 = 16
    w_scale         uint32 = 4
    w_vram          [320 * 240 * 2]byte
    w_mouse_x       int32
    w_mouse_y       int32
    w_mouse_buttons uint32
    w_signal_redraw uint8
)

//export winit
func winit() {
    w_width = 320
    w_height = 240
}

//export wupdate
func wupdate() {
    mx := int(w_mouse_x)
    my := int(w_mouse_y)
    w := int(w_width)
    
    offset := (my*w + mx) * 2
    w_vram[offset] = 0x00
    w_vram[offset+1] = 0xF8
    
    w_signal_redraw = 1
}

func main() {}
```

## Compilation

### C

```bash
clang --target=wasm32 -nostdlib -O3 -Iinclude \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    -Wl,--initial-memory=8388608 \
    main.c -o rom.wasm
```

### Rust

```bash
# Cargo.toml
[lib]
crate-type = ["cdylib"]

[profile.release]
opt-level = "z"
lto = true

# Build
cargo build --target wasm32-unknown-unknown --release
```

### Go (TinyGo)

```bash
tinygo build -o rom.wasm -target=wasm -no-debug main.go
```

### Zig

```bash
zig build-exe -target wasm32-freestanding -O ReleaseSmall main.zig
```

## Compatibility

The globals-based approach works with any WASM runtime that supports:
- WASM 1.0
- Exported linear memory
- Global variable read/write

Tested runtimes:
- ✅ wasm3 (interpreter)
- ✅ Wasmtime (JIT)
- ✅ SpiderMonkey (C++ embedding)
- ✅ Browsers (WebAssembly API)

## Performance

Globals-based access has minimal overhead:
- Global lookup: done once at initialization
- Memory read/write: direct pointer dereference (same as before)

For most games, the overhead is negligible. The Host caches global pointers after initialization, so subsequent reads/writes are just memory accesses.