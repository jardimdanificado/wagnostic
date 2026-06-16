# Wagnostic

Wagnostic is a minimalist, platform-agnostic specification and runtime for multimedia applications.

## Usage

### Quick Start

```c
#include "wagnostic.h"

void winit() {
    w_setup("Hello Wagnostic", 320, 240, 16, 2, 0);
}

void wupdate() {
    // Render something to W_FB_PTR
    w_redraw();
}
```

### Building

```bash
make             # Build Host
make -C examples # Build ROMs
./wagnostic examples/audio_test.wasm
```
---

# Full Specification

## 1. Overview

A Wagnostic ROM is a standard **WebAssembly (.wasm)** binary. The Host is any program that:
1. Instantiates the WASM module.
2. Exposes a set of **imported functions** that the ROM can call.
3. Reads and writes to the module's **linear memory** to exchange state.

All communication happens through this shared memory. All multi-byte integers are stored in **Little-Endian** format.

---

## 2. WASM Interface

### 2.1 Functions the ROM **imports**
None. Wagnostic is a pure shared-memory protocol. A ROM does not need to import any functions from the Host.

---

### 2.2 ROM must export

#### `winit()`
Called by the Host **once** immediately after instantiation. The ROM uses this to write its initial configuration into the System Header.

#### `wupdate()`
Called once per frame by the Host. The ROM reads inputs, updates state, and sets signals.

---

## 3. Memory Layout

The linear memory of the WASM instance is the single source of truth.

```
[0x000 - 0x1FF]  System Header    (512 bytes, fixed)
[0x200 ...]      VRAM             (width × height × bpp/8 bytes)
[above vram]     Audio Buffer     (audio_size bytes, ring buffer)
[above audio]    ROM Heap / RAM   (all remaining memory)
```

### 3.1 System Header (0x000 – 0x1FF)

| Offset | Size | Field | R/W | Description |
| :--- | :--- | :--- | :--- | :--- |
| 0 | 128 | **message** | ROM→Host | UTF-8 text buffer for titles, logs, etc. |
| 128 | 4 | **width** | ROM→Host | Framebuffer width in pixels. |
| 132 | 4 | **height** | ROM→Host | Framebuffer height in pixels. |
| 136 | 4 | **bpp** | ROM→Host | Bits per pixel: 8, 16, or 32. |
| 140 | 4 | **scale** | ROM→Host | Suggested window scale multiplier. |
| 144 | 4 | **audio_size** | ROM→Host | Audio ring buffer size in bytes. |
| 148 | 4 | **audio_write** | ROM→Host | ROM's write pointer (byte offset into audio buffer). |
| 152 | 4 | **audio_read** | Host→ROM | Host's read pointer (byte offset into audio buffer). |
| 156 | 4 | **audio_rate** | ROM→Host | Sample rate (e.g. 44100). |
| 160 | 4 | **audio_bpp** | ROM→Host | Audio bytes per sample (1, 2, or 4). |
| 164 | 4 | **audio_channels**| ROM→Host | Number of audio channels. |
| 168 | 4 | **ticks** | Host→ROM | Monotonic milliseconds since startup. |
| 172 | 4 | **gamepad** | Host→ROM | Button bitmask. |
| 176 | 4 | **joystick_lx** | Host→ROM | Left stick X (signed int32, -32767..32767). |
| 180 | 4 | **joystick_ly** | Host→ROM | Left stick Y axis. |
| 184 | 4 | **joystick_rx** | Host→ROM | Right stick X axis. |
| 188 | 4 | **joystick_ry** | Host→ROM | Right stick Y axis. |
| 192 | 256 | **keys** | Host→ROM | One byte per scancode. 1 = pressed. |
| 448 | 4 | **mouse_x** | Host→ROM | Mouse X in framebuffer coordinates. |
| 452 | 4 | **mouse_y** | Host→ROM | Mouse Y in framebuffer coordinates. |
| 456 | 4 | **mouse_buttons** | Host→ROM | Mouse buttons bitmask (1=L, 2=R, 4=M). |
| 460 | 4 | **mouse_wheel** | Host→ROM | Wheel delta (Host resets to 0 after write). |
| 464 | 4 | **signals** | ROM→Host | **4 fixed signal slots** (processed by Host). |
| 468 | 44 | **reserved** | - | Padding to reach 512 bytes. |

---

## 4. Video / Framebuffer

VRAM starts exactly at offset **512**.

### 8-bit: RGB332
Layout: `RRRGGGBB`. To convert to RGB888:
```c
r = ((pixel >> 5) & 0x07) * 255 / 7;
g = ((pixel >> 2) & 0x07) * 255 / 7;
b = (pixel & 0x03) * 255 / 3;
```

### 16-bit: RGB565
Stored as `uint16_t` (Little-Endian). Layout: `RRRRRGGGGGGBBBBB`.
```c
r = ((pixel >> 11) & 0x1F) * 255 / 31;
g = ((pixel >> 5) & 0x3F) * 255 / 63;
b = (pixel & 0x1F) * 255 / 31;
```

### 32-bit: RGBA8888
4 bytes per pixel: Red, Green, Blue, Alpha (Byte 0 = R, Byte 1 = G...).

---

## 5. Signals (Offset 464)

The Host must scan the 4 signal bytes after calling `wupdate`. Each processed signal **must be reset to 0**.

- `1`: **REDRAW**: ROM has finished rendering. Blit VRAM to screen.
- `2`: **QUIT**: Close the runner.
- `3`: **UPDATE_TITLE**: Set window title to string in `message`.
- `4`: **UPDATE_WINDOW**: Resize window based on header fields.
- `5`: **UPDATE_AUDIO**: Reconfigure audio device based on header fields.
- `6`: **LOG_INFO**: Print `message` to stdout.

---

## 6. Input

### 6.1 Gamepad Bitmask (Offset 172)
| Bit | Button | Bit | Button |
|---|---|---|---|
| 0 | Up | 6 | X |
| 1 | Down | 7 | Y |
| 2 | Left | 8 | L1 |
| 3 | Right | 9 | R1 |
| 4 | A | 10 | Select |
| 5 | B | 11 | Start |

### 6.2 Keyboard (Offset 192)
256 bytes indexed by USB HID Scancodes.

---

## 7. Audio

### 7.1 Ring Buffer
Starts at **512 + (width * height * bpp/8)**.
Size is `audio_size` bytes.

### 7.2 Mechanics (Host Implementation)
```c
write_ptr = read_u32(148); read_ptr = read_u32(152);
if (write_ptr >= read_ptr) available = write_ptr - read_ptr;
else available = audio_size - read_ptr + write_ptr;
```
After reading, the Host writes the new `read_ptr` to offset **152**.

---

## 8. Conformance Checklist

- [ ] Instantiates WASM with `env.get_ticks`.
- [ ] Calls `winit` once.
- [ ] Calls `wupdate` per frame.
- [ ] Keyboard: 1 byte per scancode @ 192.
- [ ] Gamepad: Bitmask @ 172.
- [ ] Signals: 4 slots @ 464.
- [ ] VRAM: Offset 512.
- [ ] Audio: Offset 512 + vram_size.
- [ ] Resets signals and mouse wheel after processing.

