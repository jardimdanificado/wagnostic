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

| Extension Identifier | Description | Struct Size | C Header |
|---|---|:---:|---|
| `std:framebuffer` | 32-bit RGBA8888 visual framebuffer (`0xAABBGGRR`) | 12 bytes | `framebuffer.h` |
| `std:clock` | Monotonic ticks, tick frequency, and delta time | 24 bytes | `clock.h` |
| `std:keyboard` | Keyboard state (256 USB HID scancodes) | 256 bytes | `keyboard.h` |
| `std:mouse` | Mouse/pointer coordinates, buttons, and wheel | 20 bytes | `mouse.h` |
| `std:gamepad` | Gamepad buttons and 8 analog axes | 20 bytes | `gamepad.h` |
| `std:gif` | GIF recording status and frame synchronization | 20 bytes | `gif.h` |
| `logger` | UTF-8 host console text logging buffer | 12 bytes | `logger.h` |

---

## 3. Extension Specifications

### 3.1 `std:framebuffer`

Provides direct access to a 32-bit RGBA8888 raster visual display.

- **Identifier**: `"std:framebuffer"` (also aliases to `"framebuffer"`, `"surface"`)
- **Total Struct Size**: `12 bytes`
- **Pixel Format**: 32-bit Little-Endian RGBA8888 (`0xAABBGGRR` / `[R, G, B, A]` in memory byte order, 4 bytes per pixel).
- **Header File**: `include/framebuffer.h`

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
- **Header File**: `include/clock.h`

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
- **Header File**: `include/keyboard.h`

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
- **Header File**: `include/mouse.h`

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

### 3.5 `std:gamepad`

Provides digital gamepad buttons and 8 analog axes.

- **Identifier**: `"std:gamepad"` (also aliases to `"gamepad"`)
- **Total Struct Size**: `20 bytes`
- **Header File**: `include/gamepad.h`

#### C Structure Definition:
```c
/* Gamepad Buttons */
#define WGAMEPAD_BTN_A             (1 << 0)
#define WGAMEPAD_BTN_B             (1 << 1)
#define WGAMEPAD_BTN_X             (1 << 2)
#define WGAMEPAD_BTN_Y             (1 << 3)
#define WGAMEPAD_BTN_LEFTSHOULDER  (1 << 4)
#define WGAMEPAD_BTN_RIGHTSHOULDER (1 << 5)
#define WGAMEPAD_BTN_SELECT        (1 << 6)
#define WGAMEPAD_BTN_START         (1 << 7)
#define WGAMEPAD_BTN_LEFTSTICK     (1 << 8)
#define WGAMEPAD_BTN_RIGHTSTICK    (1 << 9)
#define WGAMEPAD_BTN_DPAD_UP       (1 << 10)
#define WGAMEPAD_BTN_DPAD_DOWN     (1 << 11)
#define WGAMEPAD_BTN_DPAD_LEFT     (1 << 12)
#define WGAMEPAD_BTN_DPAD_RIGHT    (1 << 13)

typedef struct {
    uint32_t buttons;  /* Offset  0 (4B) - Gamepad buttons bitmask */
    int16_t  axes[8];  /* Offset  4 (16B) - 8 analog axes (-32768..32767) */
} wgamepad_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `buttons` | Host (R) | Digital gamepad buttons bitmask |
| `4` | 16 | `i16[8]` | `axes` | Host (R) | 8 analog axes (`-32768` to `32767`) |

---

### 3.6 `std:gif`

Synchronizes headless GIF animation capture and recording status between host and guest.

- **Identifier**: `"std:gif"` (also aliases to `"gif"`)
- **Total Struct Size**: `20 bytes`
- **Header File**: `include/gif.h`

#### C Structure Definition:
```c
typedef struct {
    uint32_t recording;     /* 1 if host is actively recording GIF, 0 otherwise */
    uint32_t frame_count;   /* Number of frames captured so far */
    uint32_t max_frames;    /* Max frames to record (0 = unlimited / until exit) */
    uint32_t delay_cs;      /* Frame delay in centiseconds (1/100s, e.g. 2 = 50 FPS) */
    uint32_t save_trigger;  /* ROM can set to 1 to request capturing a frame / flush */
} wgif_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `recording` | Host (R) | Recording flag (1 = active) |
| `4` | 4 | `u32` | `frame_count` | Host (R) | Number of frames recorded |
| `8` | 4 | `u32` | `max_frames` | Host (R) | Target limit frame count |
| `12` | 4 | `u32` | `delay_cs` | Host (R) | Frame delay (1/100s, e.g. 2 = 50fps) |
| `16` | 4 | `u32` | `save_trigger` | Guest (RW) | Set to 1 to trigger frame capture |

---

### 3.7 `logger`

Provides a simple UTF-8 text logging buffer to the host console.

- **Identifier**: `"logger"`
- **Total Struct Size**: `12 bytes`
- **Header File**: `include/logger.h`

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
