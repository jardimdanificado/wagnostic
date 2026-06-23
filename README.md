# Wagnostic

Wagnostic is a minimalist, platform-agnostic specification and runtime for multimedia applications.

## Quick Start

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_width = 320;
    w_height = 240;
    w_bpp = 16;
    w_scale = 4;
    w_setup("Hello Wagnostic", 320, 240, 16, 4, 0);
}

void wupdate() {
    // Read input
    int mx = w_mouse_x;
    int my = w_mouse_y;
    
    // Render to w_vram
    uint16_t* vram = (uint16_t*)w_vram;
    vram[my * w_width + mx] = W_RGB565(255, 0, 0);
    
    w_redraw();
}
```

## Building

```bash
make -C examples             # Build Host and ROMs
make -C examples host        # Build Host only
make -C examples roms        # Build ROMs only
./examples/wagnostic examples/buttons_test.wasm
```

---

# Full Specification

## 1. Overview

A Wagnostic ROM is a regular **WebAssembly (.wasm)** binary. The Host is any program that:
1. Instantiates the WASM module.
2. Reads and writes to the module's **named globals** to exchange state.
3. Calls the ROM's exported functions (`winit` and `wupdate`).

All communication happens through named globals. Each global contains a pointer (i32) into WASM linear memory.

---

## 2. WASM Interface

### 2.1 ROM must export

#### `winit()`
Called by the Host **once** immediately after instantiation. The ROM uses this to set its initial configuration via globals.

#### `wupdate()`
Called once per frame by the Host. The ROM reads inputs, updates state, and sets signals. **All signal writes (including REDRAW) must happen inside `wupdate()`.**

---

## 3. Named Globals

The ROM exports named global variables. Each global contains an i32 pointer into WASM linear memory. The host reads/writes the actual value by dereferencing this pointer.

### 3.1 Screen Configuration (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_width` | `uint32` | Screen width in pixels |
| `w_height` | `uint32` | Screen height in pixels |
| `w_bpp` | `uint32` | Bits per pixel (8, 16, or 32) |
| `w_scale` | `uint32` | Window scale factor |
| `w_title` | `char[128]` | Window title (null-terminated) |

### 3.2 VRAM (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_vram` | `uint8[]` | Pixel buffer (size: `w_width * w_height * w_bpp / 8`) |

### 3.3 Input (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_mouse_x` | `int32` | Mouse X position |
| `w_mouse_y` | `int32` | Mouse Y position |
| `w_mouse_buttons` | `uint32` | Mouse buttons bitmask (bit 0=L, bit 1=R) |
| `w_mouse_wheel` | `int32` | Mouse wheel delta |
| `w_keys` | `uint8[256]` | Keyboard state (USB HID scancodes) |
| `w_gamepad_buttons` | `uint32` | Gamepad button state |

### 3.4 Timing (Host writes, ROM reads)

| Name | Type | Description |
|------|------|-------------|
| `w_ticks` | `uint32` | Time in milliseconds since startup |

### 3.5 Audio (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_audio_size` | `uint32` | Audio buffer size in bytes |
| `w_audio_sample_rate` | `uint32` | Sample rate (Hz) |
| `w_audio_bpp` | `uint32` | Bytes per sample (1, 2, or 4) |
| `w_audio_channels` | `uint32` | Number of channels |
| `w_audio_write` | `uint32` | Write position (ring buffer) |
| `w_audio_read` | `uint32` | Read position (ring buffer) |
| `w_audio_buffer` | `uint8[]` | Audio ring buffer |

### 3.6 Signals (ROM writes, Host reads)

| Name | Type | Description |
|------|------|-------------|
| `w_signal_redraw` | `uint8` | Request screen redraw (1=redraw) |
| `w_signal_quit` | `uint8` | Request quit (1=quit) |
| `w_signal_update_window` | `uint8` | Request window update |
| `w_signal_update_audio` | `uint8` | Request audio reinit |

---

## 4. Video / Framebuffer

### 8-bit: RGB332
Layout: `RRRGGGBB`.
```c
// Encoding (RGB888 → RGB332)
pixel = ((r & 0xE0) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6));

// Decoding (RGB332 → RGB888)
r = ((pixel >> 5) & 0x07) * 255 / 7;
g = ((pixel >> 2) & 0x07) * 255 / 7;
b = (pixel & 0x03) * 255 / 3;
```

### 16-bit: RGB565
Stored as `uint16_t` (Little-Endian). Layout: `RRRRRGGGGGGBBBBB`.
```c
// Encoding (RGB888 → RGB565)
pixel = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

// Decoding (RGB565 → RGB888)
r = ((pixel >> 11) & 0x1F) * 255 / 31;
g = ((pixel >> 5) & 0x3F) * 255 / 63;
b = (pixel & 0x1F) * 255 / 31;
```

### 32-bit: RGBA8888
4 bytes per pixel: Red, Green, Blue, Alpha (Byte 0 = R, Byte 1 = G, Byte 2 = B, Byte 3 = A).
```c
// Encoding (RGB888 + Alpha → RGBA8888)
pixel = (a << 24) | (b << 16) | (g << 8) | r;

// Decoding (RGBA8888 → components)
r = (pixel >> 0) & 0xFF;
g = (pixel >> 8) & 0xFF;
b = (pixel >> 16) & 0xFF;
a = (pixel >> 24) & 0xFF;
```

---

## 5. Signals

The Host must scan signal globals after calling `wupdate`. Each processed signal **must be reset to 0**.

- `w_signal_redraw = 1`: ROM has finished rendering. Blit VRAM to screen.
- `w_signal_quit = 1`: Close the runner.
- `w_signal_update_window = 1`: Resize window based on config globals.
- `w_signal_update_audio = 1`: Reconfigure audio device based on config globals.

---

## 6. Input

### 6.1 Gamepad Bitmask (`w_gamepad_buttons`)
| Bit | Button | Bit | Button |
|---|---|---|---|
| 0 | Up | 6 | X |
| 1 | Down | 7 | Y |
| 2 | Left | 8 | L1 |
| 3 | Right | 9 | R1 |
| 4 | A | 10 | Select |
| 5 | B | 11 | Start |
| 12 | L2 | 13 | R2 |

### 6.2 Keyboard (`w_keys`)
256 bytes indexed by USB HID Scancodes. 1 = pressed, 0 = released.

### 6.3 Mouse

| Name | Type | Description |
|---|---|---|
| `w_mouse_x` | `int32` | Mouse X in framebuffer coordinates |
| `w_mouse_y` | `int32` | Mouse Y in framebuffer coordinates |
| `w_mouse_buttons` | `uint32` | Mouse buttons bitmask (bit 0 = L, bit 1 = R, bit 2 = M) |
| `w_mouse_wheel` | `int32` | Wheel delta (Host resets to 0 after each frame) |

---

## 7. Audio

### 7.1 Ring Buffer
Size is `w_audio_size` bytes.

### 7.2 Audio Format

| w_audio_bpp | Format | Range |
|---|---|---|
| 1 | Unsigned 8-bit PCM | 0–255 (128 = silence) |
| 2 | Signed 16-bit PCM (Little-Endian) | -32768 to 32767 |
| 4 | 32-bit float (Little-Endian) | -1.0 to 1.0 |

Multi-channel audio is **interleaved** (L, R, L, R, ...).

### 7.3 ROM Writing Audio

```c
uint16_t* audio_buf = (uint16_t*)w_audio_buffer;
uint32_t wp = w_audio_write;
uint32_t size = w_audio_size;

// Write samples
audio_buf[wp / 2] = sample;
wp = (wp + 2) % size;

w_audio_write = wp;
```

---

## 8. Conformance Checklist

- [ ] Instantiates WASM with no imports (except optional strlen).
- [ ] Calls `winit` once after instantiation.
- [ ] Calls `wupdate` per frame.
- [ ] Finds globals by name using WASM API.
- [ ] Keyboard: `w_keys` array indexed by USB HID scancodes.
- [ ] Gamepad: `w_gamepad_buttons` bitmask.
- [ ] Mouse: `w_mouse_x`, `w_mouse_y`, `w_mouse_buttons`, `w_mouse_wheel`.
- [ ] Signals: `w_signal_redraw`, `w_signal_quit`, `w_signal_update_window`, `w_signal_update_audio`.
- [ ] VRAM: `w_vram` global.
- [ ] Audio: `w_audio_buffer` global.
- [ ] Resets all signals after processing.
- [ ] Resets `w_mouse_wheel` to 0 after each `wupdate()` call.