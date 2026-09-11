# Piolho 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

- 📜 **[ABI.md](ABI.md)**: Core Binary ABI specification (`update`, `use`, execution lifecycle).
- 🧩 **[STD.md](STD.md)**: Standard Extensions specification (`std:framebuffer`, `std:clock`, `std:keyboard`, `std:mouse`, `std:gamepad`, `std:gif`, `logger`, `comm:*`).
- 🔄 **[IPC.md](IPC.md)**: Multi-ROM Worker & Synchronous Rendezvous IPC specification (`tell`, `hear`).

---

## Quick Start (Using Piolho as a Library)

Piolho is a zero-dependency WebAssembly runtime library for Node.js, `tjs` (txiki.js), Bun, and Deno:

```javascript
const { Piolho, framebufferExtension, clockExtension } = require('piolho');

async function main() {
  const host = new Piolho(); // 0 extensions by default
  host.use(framebufferExtension).use(clockExtension); // opt-in extensions

  await host.loadRom('roms/display_test.wasm', 'display');
  await host.run(60); // run for 60 steps (or omit to run continuously)
}

main();
```

---

## Command Line Interface (CLI)

The CLI allows running ROMs directly from the terminal with opt-in extensions, custom extension search paths, and multi-instance IPC:

```bash
# Run bare ROM with default communication extensions
piolho app.wasm

# Run multi-worker with explicit IPC aliases
piolho master.wasm:master worker.wasm:worker

# Explicitly opt-in to standard extensions
piolho -e clock,framebuffer display.wasm:ui

# Search for extensions dynamically in directories
piolho -E ./my_extensions -e custom_dsp worker.wasm

# Limit execution steps
piolho -s 100 benchmark.wasm
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
| `comm:workers` | Worker sub-instance capability indicator (returns 1 or 0) | 0 B | `comm_workers.h` |

---

## Running Test Suite

```bash
npm test
# ou:
make -C roms test
```
