# Wagnostic 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

- **[ABI.md](ABI.md)**: Core Binary ABI specification (`update`, `ask`, execution lifecycle).
- **[STD.md](STD.md)**: Standard Extensions specification (`std:framebuffer`, `std:clock`, `std:keyboard`, `std:mouse`, `logger`).

---

## Quick Start

### 1. Native CLI Runner & GIF Exporter (100% libc / POSIX C + wasm3)
Zero windowing dependencies:
```bash
# Build native runner:
make -C runners/native

# Headless execution & GIF animation export:
./runners/native/wagnostic roms/display_test.wasm -o demo.gif -n 60
```

### 2. Embeddable JavaScript / Web Runner (`runners/js/`)
Zero-dependency single-file ES6 module for Browser, Node.js, Deno, and Bun:
```javascript
import { Wagnostic } from './runners/js/wagnostic.js';
import { handleExtension, updateStd, getFramebuffer } from './runners/js/std.js';

const runner = new Wagnostic(handleExtension);
await runner.init(wasmBytes);

// In your game/frame loop:
updateStd(runner);
runner.step(); // calls update()
const fb = getFramebuffer(runner);
```

### 3. Web Host with `<canvas>` (`runners/web/`)
Open `runners/web/index.html` in your browser (via local web server or file picker) to run ROMs with full canvas rendering, keyboard and mouse controls.

---

## Standard Extensions Summary

For full memory layouts, struct fields, and byte offsets, see **[STD.md](STD.md)**.

| Extension Name | Description | Size |
|---|---|:---:|
| `std:framebuffer` | Direct 32-bit RGBA8888 framebuffer (`0xAABBGGRR`) and dimensions | 12 B |
| `std:clock` | Monotonic ticks, frequency, and frame delta time | 24 B |
| `std:keyboard` | Keyboard state (256 USB HID scancodes) | 256 B |
| `std:mouse` | Mouse coordinates (x, y), button bitmask, and wheel scroll deltas | 20 B |
| `logger` | Simple UTF-8 text message logging to host console | 12 B |

---

## Running Test Suite

```bash
make -C roms test-native   # Runs all test ROMs through native runner
make -C roms test-node     # Runs all test ROMs through Node.js runner
```
