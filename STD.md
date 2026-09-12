# Wagnostic 2.0 — Standard Extensions Specification (`std:*`)

This document defines the official standard extension structures (`std:*`) for Wagnostic 2.0.

All extensions in Wagnostic are completely **optional** and **modular**. A host is only required to implement the extensions it can support, and a guest ROM must gracefully check if an extension pointer is non-null before accessing its memory.

---

## 1. Extension Struct Conventions

Every standard extension structure adheres to the following conventions:

1. **No Mandatory Metadata**: Structs contain only the essential fields needed for the capability without artificial headers.
2. **Endianness**: All numeric values (integers and floats) use **32-bit Little-Endian** encoding.
3. **Alignment**: Structures are aligned to 4-byte boundaries (or 8-byte for 64-bit fields).
4. **Pointers**: Any pointers within structures (`pixels`, `buffer`, etc.) are **32-bit byte offsets** into the guest WebAssembly module's linear memory.
5. **Field Access**:
   - Fields marked **Host (R)** are written by the host and read-only to the guest.
   - Fields marked **Host/Guest (RW)** can be negotiated or written by either party according to the extension contract.
   - Fields marked **Guest (RW)** are written by the guest module.

---

## 2. Standard Extensions Summary

| Extension Identifier | Description | Struct Size |
|---|---|:---:|
| `std:framebuffer` | 32-bit RGBA8888 visual framebuffer (`0xAABBGGRR`) | 12 bytes |
| `std:clock` | Monotonic ticks, tick frequency, and delta time | 24 bytes |
| `std:keyboard` | Keyboard state (256 USB HID scancodes) | 256 bytes |
| `std:mouse` | Mouse/pointer coordinates, buttons, and wheel | 20 bytes |
| `logger` | UTF-8 host console text logging buffer | 12 bytes |

---

## 3. Extension Specifications

### 3.1 `std:framebuffer`

Provides direct access to a 32-bit RGBA8888 raster visual display.

- **Identifier**: `"std:framebuffer"` (also aliases to `"framebuffer"`, `"surface"`)
- **Total Struct Size**: `12 bytes`
- **Pixel Format**: 32-bit Little-Endian RGBA8888 (`0xAABBGGRR` / `[R, G, B, A]` in memory byte order, 4 bytes per pixel).

#### C Structure Definition:
```c
typedef struct {
    uint32_t width;   /* Framebuffer width in pixels */
    uint32_t height;  /* Framebuffer height in pixels */
    uint32_t pixels;  /* WASM pointer to 32-bit RGBA pixel buffer */
} wframebuffer_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `width` | Host/Guest (RW) | Framebuffer width in pixels |
| `4` | 4 | `u32` | `height` | Host/Guest (RW) | Framebuffer height in pixels |
| `8` | 4 | `u32` | `pixels` | Host/Guest (RW) | Byte offset to pixel buffer in WASM memory |

---

### 3.2 `std:clock`

Provides high-precision monotonic timing and frame delta calculations.

- **Identifier**: `"std:clock"` (also aliases to `"clock"`)
- **Total Struct Size**: `24 bytes`

#### C Structure Definition:
```c
typedef struct {
    uint64_t ticks;      /* Total monotonic ticks elapsed */
    uint64_t frequency;  /* Ticks per second (e.g. 1000 for ms) */
    float    delta;      /* Elapsed seconds since last frame */
} wclock_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 8 | `u64` | `ticks` | Host (R) | Monotonic tick counter |
| `8` | 8 | `u64` | `frequency` | Host (R) | Clock frequency (ticks per second) |
| `16` | 4 | `f32` | `delta` | Host (R) | Delta time in seconds since previous frame |

---

### 3.3 `std:keyboard`

Provides keyboard input state representing 256 standard USB HID scancodes.

- **Identifier**: `"std:keyboard"` (also aliases to `"keyboard"`)
- **Total Struct Size**: `256 bytes`

#### C Structure Definition:
```c
typedef struct {
    uint8_t keys[256]; /* Offset 0 (256B) - USB HID scancodes (0=up, 1=down) */
} wkeyboard_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 256 | `u8[256]` | `keys` | Host (R) | Standard USB HID scancode table (0=up, 1=down) |

---

### 3.4 `std:mouse`

Provides mouse / pointer coordinates, button bitmask, and 2-axis wheel scroll deltas.

- **Identifier**: `"std:mouse"` (also aliases to `"mouse"`)
- **Total Struct Size**: `20 bytes`

#### C Structure Definition:
```c
/* Mouse Buttons */
#define WMOUSE_BTN_LEFT   (1 << 0)
#define WMOUSE_BTN_RIGHT  (1 << 1)
#define WMOUSE_BTN_MIDDLE (1 << 2)

typedef struct {
    int32_t  x;          /* Offset  0 (4B) - Cursor X coordinate */
    int32_t  y;          /* Offset  4 (4B) - Cursor Y coordinate */
    uint32_t buttons;    /* Offset  8 (4B) - Buttons bitmask (1=L, 2=R, 4=M) */
    int32_t  wheel_x;    /* Offset 12 (4B) - Horizontal scroll delta */
    int32_t  wheel_y;    /* Offset 16 (4B) - Vertical scroll delta */
} wmouse_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `i32` | `x` | Host (R) | Mouse/pointer X coordinate in pixels |
| `4` | 4 | `i32` | `y` | Host (R) | Mouse/pointer Y coordinate in pixels |
| `8` | 4 | `u32` | `buttons` | Host (R) | Mouse button bitmask |
| `12` | 4 | `i32` | `wheel_x` | Host (R) | Horizontal wheel delta |
| `16` | 4 | `i32` | `wheel_y` | Host (R) | Vertical wheel delta |

---

### 3.5 `logger`

Provides a simple UTF-8 text logging buffer to the host console.

- **Identifier**: `"logger"`
- **Total Struct Size**: `12 bytes`

#### C Structure Definition:
```c
typedef struct {
    uint32_t buffer;      /* WASM memory pointer to UTF-8 text buffer */
    uint32_t capacity;    /* Capacity in bytes */
    uint32_t length;      /* Length of text written by ROM (host clears to 0 after printing) */
} wlogger_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `buffer` | Host (R) | Byte offset to UTF-8 text buffer in WASM memory |
| `4` | 4 | `u32` | `capacity` | Host (R) | Buffer capacity in bytes |
| `8` | 4 | `u32` | `length` | Guest (RW) | Number of bytes written by guest |
