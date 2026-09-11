# Piolho

Minimalist, modular, platform-agnostic WebAssembly multi-instance runtime and rendezvous communication coordinator.

- **[ABI.md](ABI.md)**: Binary ABI specification.
---

## 1. Quick Start (Library Usage)

Piolho is a zero-dependency WebAssembly runtime library for Node.js, `tjs` (txiki.js), Bun, and Deno:

```javascript
const { Piolho, clockExtension, loggerExtension } = require('piolho');

async function main() {
  const host = new Piolho(); // 0 extensions by default
  host.use(clockExtension).use(loggerExtension); // opt-in extensions

  await host.loadRom('roms/ipc_producer.wasm', 'producer');
  await host.loadRom('roms/ipc_consumer.wasm', 'consumer');
  await host.run(100); // run for 100 steps (or omit to run continuously)
}

main();
```

---

## 2. Command Line Interface (CLI)

The CLI allows running ROMs directly from the terminal with opt-in extensions, custom extension search paths, and multi-instance IPC:

```bash
# Run bare ROM with default communication extensions
piolho worker.wasm

# Run multi-worker with explicit IPC aliases
piolho master.wasm:master worker.wasm:worker

# Explicitly opt-in to extensions
piolho -e clock,logger app.wasm

# Search for extensions (name.js) dynamically in directories
piolho -E ./my_extensions -e custom_dsp worker.wasm

# Limit execution steps
piolho -s 100 benchmark.wasm
```

---

## 3. Architecture Overview

In Piolho, modules export a single lifecycle function `update()` and communicate transparently using rendezvous IPC (`tell` / `hear`):

```c
#include "piolho.h"
#include "clock.h"
#include "logger.h"
#include "comm_workers.h"

static clock_ext_t *clock_ext = 0;

int32_t update(void) {
    if (!clock_ext) {
        clock_ext = (clock_ext_t*)ask("clock");
    }
    uint32_t data = 42;
    tell("worker", &data, sizeof(data), 0);
    return UPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
}
```

For the complete binary specifications, memory layouts, and communication, see **[ABI.md](ABI.md)**.

---

## 4. Running Test Suite

```bash
npm test
# ou:
make -C roms test
```
