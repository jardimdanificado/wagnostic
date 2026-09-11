# Piolho 2.0

Minimalist, modular, platform-agnostic WebAssembly multi-instance runtime and communication protocol.

- 📜 **[ABI.md](ABI.md)**: Core Binary ABI specification (`update`, `use`, execution lifecycle).
- 🧩 **[STD.md](STD.md)**: Standard Extensions specification (`std:clock`, `logger`, `comm:*`).
- 🔄 **[IPC.md](IPC.md)**: Multi-ROM Worker & Synchronous Rendezvous IPC specification (`tell`, `hear`).

---

## Quick Start (Using Piolho as a Library)

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

## Command Line Interface (CLI)

The CLI allows running ROMs directly from the terminal with opt-in extensions, custom extension search paths, and multi-instance IPC:

```bash
# Run bare ROM with default communication extensions
piolho worker.wasm

# Run multi-worker with explicit IPC aliases
piolho master.wasm:master worker.wasm:worker

# Explicitly opt-in to standard extensions
piolho -e clock,logger app.wasm

# Search for extensions (name.js) dynamically in directories
piolho -E ./my_extensions -e custom_dsp worker.wasm

# Limit execution steps
piolho -s 100 benchmark.wasm
```

---

## Architecture Overview

In Piolho 2.0, modules export a single lifecycle function `update()` and request capabilities dynamically via named extensions using `use()`:

```c
#include "piolho.h"
#include "clock.h"
#include "logger.h"
#include "comm_workers.h"

static clock_ext_t *clock_ext;

int32_t setup(void) {
    clock_ext = (clock_ext_t*)use("std:clock");
    int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
    return 0;
}

int32_t update(void) {
    uint32_t data = 42;
    tell("worker", &data, sizeof(data), 0);
    return UPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
}
```

---

## Standard Extensions Summary

For full memory layouts, struct fields, and specifications, see **[STD.md](STD.md)**.

| Extension Name | Description | Size | Header |
|---|---|:---:|---|
| `std:clock` | Monotonic ticks, frequency, and step delta time | 24 B | `clock.h` |
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
