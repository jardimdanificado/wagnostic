# Wagnostic 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

- 📜 **[ABI.md](ABI.md)**: Core Binary ABI specification (`wupdate`, `wextension`, execution lifecycle).
- 🧩 **[STD.md](STD.md)**: Standard Extensions specification (`std:framebuffer`, `std:clock`, `std:keyboard`, `std:mouse`, `std:gamepad`, `std:gif`, `logger`).
- 🔄 **[IPC.md](IPC.md)**: Multi-ROM Worker & Synchronous Rendezvous IPC specification (`wask`, `wtell`).

---

## Quick Start

### 1. Native Runner (100% libc / POSIX C + wasm3)
Zero windowing dependencies (pure ANSI TrueColor terminal renderer + GIF export):
```bash
# Build native runner:
mkdir -p build && cd build && cmake .. && cmake --build .
# Or via runners/native/Makefile:
make -C runners/native

# Interactive Terminal Execution:
./build/wagnostic ../roms/display_test.wasm

# Headless Execution & GIF Export:
./build/wagnostic -g output.gif -n 60 ../roms/display_test.wasm
```

### 2. Node.js & Txiki.js Runner (Zero npm dependencies)
Works out of the box with Node.js or `tjs` (txiki.js):
```bash
# Interactive Terminal Run:
node runners/node/wagnostic.js roms/display_test.wasm

# With Txiki:
tjs run runners/node/wagnostic.js roms/display_test.wasm

# Headless GIF Export:
node runners/node/wagnostic.js -g output.gif -n 60 roms/display_test.wasm
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

## Runners & Templates
 
1. **Official Runners (`runners/`)**:
   - **`runners/native/`**: 100% `libc` / POSIX C runner with wasm3. Renders directly in terminal with 24-bit ANSI TrueColor half-blocks (`▀`) and headless GIF encoder (`-g file.gif`). Zero SDL2 / OpenGL dependencies!
   - **`runners/node/`**: Universal zero-dependency JavaScript runner compatible with both **Node.js** and **txiki.js (`tjs`)**. ANSI TrueColor terminal renderer + pure JS GIF encoder.
2. **Bare Reference Templates (`examples/`)**:
   - **`examples/bare_runner.js`**: Pure JavaScript host (~60 lines) with custom extension dispatch.
   - **`examples/bare_runner.c`**: Pure C host using wasm3 (~90 lines).
 
---
 
## Running Test Suite
 
```bash
cd roms
make test-native   # Runs all 15 test ROMs through native runner
make test-node     # Runs all 15 test ROMs through Node.js runner
```
