# Piolho 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

- 📜 **[ABI.md](ABI.md)**: Core Binary ABI specification (`update`, `use`, execution lifecycle).
- 🧩 **[STD.md](STD.md)**: Standard Extensions specification (`std:framebuffer`, `std:clock`, `std:keyboard`, `std:mouse`, `std:gamepad`, `std:gif`, `logger`, `comm:*`).
- 🔄 **[IPC.md](IPC.md)**: Multi-ROM Worker & Synchronous Rendezvous IPC specification (`tell`, `hear`).

---

## Quick Start

### 1. Universal Host (Node.js & txiki.js — Zero External Dependencies)
The primary and most portable host ecosystem. Works out of the box with Node.js, `tjs` (txiki.js), Bun, or Deno:
```bash
# Run with Node.js (via root CLI or npm start):
node bin/piolho.js roms/display_test.wasm

# Run with txiki.js:
tjs bin/piolho.js roms/display_test.wasm

# Multi-ROM Parallel Path Tracer with 2 Workers & GIF Export:
node bin/piolho.js roms/pathtracer_master.wasm:master \
                   roms/pathtracer_worker.wasm:worker0 \
                   roms/pathtracer_worker.wasm:worker1 -g render.gif -n 30
```

---

## Architecture Overview

In Piolho 2.0, modules export a single lifecycle function `update()` and request capabilities dynamically via named extensions using `use()`:

```c
#include "piolho.h"
#include "framebuffer.h"
#include "clock.h"

static framebuffer_t *fb;
static clock_ext_t   *clock_ext;

int32_t setup(void) {
    fb        = (framebuffer_t*)use("std:framebuffer");
    clock_ext = (clock_ext_t*)use("std:clock");
    if (fb) {
        fb->width  = 320;
        fb->height = 240;
    }
    return 0;
}

int32_t update(void) {
    if (fb && fb->pixels) {
        uint32_t *pixels = (uint32_t*)fb->pixels;
        // Draw 32-bit RGBA8888 pixels (0xAABBGGRR)...
    }

    return UPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
}
```

---

## Standard Extensions Summary

For full memory layouts, struct fields, and specifications, see **[STD.md](STD.md)**.

| Extension Name | Description | Size | Header |
|---|---|:---:|---|
| `std:framebuffer` | Direct 32-bit RGBA8888 framebuffer (`0xAABBGGRR`) and dimensions | 12 B | `framebuffer.h` |
| `std:clock` | Monotonic ticks, frequency, and frame delta time | 24 B | `clock.h` |
| `std:keyboard` | Keyboard state (256 USB HID scancodes) | 256 B | `keyboard.h` |
| `std:mouse` | Mouse coordinates (x, y), button bitmask, and wheel scroll deltas | 20 B | `mouse.h` |
| `std:gamepad` | Gamepad digital buttons and 8 analog axes | 20 B | `gamepad.h` |
| `std:gif` | GIF recording status, frame count, delay, and frame capture synchronization | 20 B | `gif.h` |
| `logger` | Simple UTF-8 text message logging to host console | 12 B | `logger.h` |
| `comm:tcp` | TCP peer discovery, active/passive binding, and transparent IPC | 144 B | `comm_tcp.h` |
| `comm:pipe` | Unix domain socket / named pipe discovery and transparent IPC | 204 B | `comm_pipe.h` |
| `comm:ws` | WebSocket client/server discovery and transparent IPC | 208 B | `comm_ws.h` |
| `comm:udp` | UDP datagram probing, beacon broadcast, and transparent IPC | 144 B | `comm_udp.h` |

---

## Running Test Suite

```bash
npm test
# ou:
make -C roms test
```
