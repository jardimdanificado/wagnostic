# Wagnostic 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

- 📜 **[ABI.md](ABI.md)**: Core Binary ABI specification (`wupdate`, `wextension`, execution lifecycle).
- 🧩 **[STD.md](STD.md)**: Standard Extensions specification (`std:framebuffer`, `std:clock`, `std:keyboard`, `std:mouse`, `std:gamepad`, `std:gif`, `logger`).
- 🔄 **[IPC.md](IPC.md)**: Multi-ROM Worker & Synchronous Rendezvous IPC specification (`wask`, `wtell`).

---

## Quick Start

### 1. Universal Host (Node.js & txiki.js — Zero External Dependencies)
The primary and most portable host ecosystem. Works out of the box with Node.js, `tjs` (txiki.js), Bun, or Deno:
```bash
# Run with Node.js (via root CLI or npm start):
node bin/wagnostic.js roms/display_test.wasm

# Run with txiki.js:
tjs bin/wagnostic.js roms/display_test.wasm

# Multi-ROM Parallel Path Tracer with 4 Workers & GIF Export:
node bin/wagnostic.js roms/pathtracer_master.wasm:master \
                      roms/pathtracer_worker.wasm:worker0 \
                      roms/pathtracer_worker.wasm:worker1 \
                      roms/pathtracer_worker.wasm:worker2 \
                      roms/pathtracer_worker.wasm:worker3 -g render.gif -n 30
```

### 2. Native C Host (100% libc / POSIX C + wasm3)
Native C host for bare environments without JavaScript runtime:
```bash
# Build native runner:
mkdir -p build && cd build && cmake .. && cmake --build .

# Run native host:
./build/wagnostic roms/display_test.wasm -g output.gif -n 30
```

---

## Architecture Overview

In Wagnostic 2.0, modules export a single lifecycle function `wupdate()` and request capabilities dynamically via named extensions:

```c
#include "wagnostic.h"
#include "framebuffer.h"
#include "clock.h"

static wframebuffer_t *fb;
static wclock_t       *clock_ext;

int32_t wupdate(void) {
    if (!fb) {
        fb        = (wframebuffer_t*)wextension("std:framebuffer");
        clock_ext = (wclock_t*)wextension("std:clock");
        if (fb) {
            fb->width  = 320;
            fb->height = 240;
        }
    }

    if (fb && fb->pixels) {
        uint32_t *pixels = (uint32_t*)fb->pixels;
        // Draw 32-bit RGBA8888 pixels (0xAABBGGRR)...
    }

    return WUPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
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

---

## Runners & Architecture
 
1. **Universal Host (`bin/wagnostic.js`, `src/`)**:
   - Universal zero-dependency JavaScript host compatible with **Node.js**, **txiki.js (`tjs`)**, **Bun**, and **Deno**.
   - Modular Extension Registry (`ExtensionRegistry`), Multi-ROM Rendezvous IPC, and pure JS GIF encoder.
2. **Native C Host (`runners/native/`, `build/wagnostic`)**:
   - 100% `libc` / POSIX C runner with wasm3. Multi-threaded OS worker pool with zero external runtime dependencies.
3. **Bare Reference Templates (`examples/`)**:
   - **`examples/bare_runner.js`**: Minimal standalone JavaScript host (~60 lines).
   - **`examples/bare_runner.c`**: Minimal standalone C host using wasm3 (~90 lines).
 
---
 
## Running Test Suite
 
```bash
cd roms
make test-native   # Runs all 15 test ROMs through native runner
make test-node     # Runs all 15 test ROMs through Node.js runner
```
