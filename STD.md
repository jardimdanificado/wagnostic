# Piolho 2.0 — Standard Extensions Specification (`std:*`)

This document defines the official standard extension structures (`std:*`) for Piolho 2.0.

All extensions in Piolho are completely **optional** and **modular**. A host is only required to implement the extensions it can support, and a guest ROM must gracefully check if an extension pointer is non-null before accessing its memory.

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
| `comm:tcp` | TCP peer discovery, probing, and binding | 108 bytes | `comm_tcp.h` |
| `comm:pipe` | Unix domain socket / named pipe peer discovery | 168 bytes | `comm_pipe.h` |
| `comm:ws` | WebSocket client/server discovery & binding | 172 bytes | `comm_ws.h` |
| `comm:udp` | UDP datagram probing & beacon discovery | 108 bytes | `comm_udp.h` |

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
} framebuffer_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `width` | Host/Guest (RW) | Display width in pixels |
| `4` | 4 | `u32` | `height` | Host/Guest (RW) | Display height in pixels |
| `8` | 4 | `u32` | `pixels` | Host (R) | Byte offset to 32-bit RGBA pixel buffer in WASM memory |

---

### 3.2 `std:clock`

Provides monotonic high-precision timing and frame delta calculations.

- **Identifier**: `"std:clock"` (also aliases to `"clock"`)
- **Total Struct Size**: `24 bytes`
- **Header File**: `include/clock.h`

#### C Structure Definition:
```c
typedef struct {
    uint64_t ticks;      /* Monotonic tick count */
    uint64_t frequency;  /* Ticks per second */
    double   delta_time; /* Time elapsed since last frame in seconds */
} clock_ext_t;
```

#### Memory Layout:
| Offset | Size | Type | Field | Access | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 8 | `u64` | `ticks` | Host (R) | Monotonic counter |
| `8` | 8 | `u64` | `frequency` | Host (R) | Ticks per second |
| `16` | 8 | `f64` | `delta_time` | Host (R) | Elapsed seconds since previous frame |

---

### 3.3 `std:keyboard`

Provides access to 256 physical keyboard key states mapped directly to standard USB HID Usage IDs.

- **Identifier**: `"std:keyboard"` (also aliases to `"keyboard"`)
- **Total Struct Size**: `256 bytes`
- **Header File**: `include/keyboard.h`

#### C Structure Definition:
```c
typedef struct {
    uint8_t keys[256]; /* 1 = Key Down, 0 = Key Up (Indexed by USB HID scancode) */
} keyboard_t;
```

---

### 3.4 `std:mouse`

Provides 2D mouse cursor coordinates, digital button states, and wheel scrolling deltas.

- **Identifier**: `"std:mouse"` (also aliases to `"mouse"`)
- **Total Struct Size**: `20 bytes`
- **Header File**: `include/mouse.h`

#### C Structure Definition:
```c
typedef struct {
    int32_t x;          /* Mouse X cursor coordinate */
    int32_t y;          /* Mouse Y cursor coordinate */
    uint32_t buttons;   /* Bitmask of active mouse buttons */
    int32_t wheel_x;    /* Horizontal scroll wheel delta */
    int32_t wheel_y;    /* Vertical scroll wheel delta */
} mouse_t;
```

---

### 3.5 `std:gamepad`

Provides digital button state and 8 analog axes for standard game controllers.

- **Identifier**: `"std:gamepad"` (also aliases to `"gamepad"`)
- **Total Struct Size**: `20 bytes`
- **Header File**: `include/gamepad.h`

#### C Structure Definition:
```c
typedef struct {
    uint32_t buttons;  /* Bitmask of digital gamepad buttons */
    int16_t  axes[8];  /* 8 analog axis channels (-32768 to 32767) */
} gamepad_t;
```

---

### 3.6 `std:gif`

Controls GIF animation recording and frame capture synchronization.

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
} gif_t;
```

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
} logger_t;
```

---

### 3.8 `comm:tcp`

Enables explicit discovery and binding to remote Piolho instances over TCP. Once connected, instances communicate transparently using standard `tell` / `hear` by name.

- **Identifier**: `"comm:tcp"`
- **Total Struct Size**: `108 bytes`
- **Header File**: `include/comm_tcp.h`

#### C Structure Definition:
```c
typedef struct {
    char host[64];       /* Target host/IP or bind address */
    int32_t port;        /* TCP Port number */
    int32_t mode;        /* 0 = connect (client), 1 = listen (server) */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    char peer_name[32];  /* Discovered peer name (filled by host on connect) */
} comm_tcp_t;
```

---

### 3.9 `comm:pipe`

Enables discovery and binding to local Piolho instances over Unix domain sockets or named pipes.

- **Identifier**: `"comm:pipe"`
- **Total Struct Size**: `168 bytes`
- **Header File**: `include/comm_pipe.h`

#### C Structure Definition:
```c
typedef struct {
    char path[128];      /* Unix socket or named pipe path */
    int32_t mode;        /* 0 = connect, 1 = listen */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    char peer_name[32];  /* Discovered peer name */
} comm_pipe_t;
```

---

### 3.10 `comm:ws`

Enables discovery and binding to remote WebSocket endpoints.

- **Identifier**: `"comm:ws"`
- **Total Struct Size**: `172 bytes`
- **Header File**: `include/comm_ws.h`

#### C Structure Definition:
```c
typedef struct {
    char url[128];       /* WebSocket URL (ws://... or wss://...) */
    int32_t port;        /* Listen port if mode == 1 */
    int32_t mode;        /* 0 = connect, 1 = listen */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    char peer_name[32];  /* Discovered peer name */
} comm_ws_t;
```

---

### 3.11 `comm:udp`

Enables UDP probing, beacon discovery, and datagram binding.

- **Identifier**: `"comm:udp"`
- **Total Struct Size**: `108 bytes`
- **Header File**: `include/comm_udp.h`

#### C Structure Definition:
```c
typedef struct {
    char host[64];       /* Target host or broadcast address */
    int32_t port;        /* UDP port */
    int32_t mode;        /* 0 = probe/client, 1 = listen/server */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    char peer_name[32];  /* Discovered peer name */
} comm_udp_t;
```
