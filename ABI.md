# Piolho — Binary ABI Specification

This document defines the core binary Application Binary Interface (ABI) of **Piolho**.

The Piolho core ABI is an ultra-minimalist, host-agnostic, and language-neutral specification. It establishes only the basic execution lifecycle and capability negotiation mechanism between a host and a guest WebAssembly module.
---

## 1. Core Execution Model

A Piolho module is a standard 32-bit WebAssembly (Wasm MVP) binary with a linear memory.

The entire interaction between host and guest is governed by exactly **two functions**:
1. **One exported entry point**: `update()` (called by the host).
2. **One imported capability dispatcher**: `use(name)` (called by the guest).

```
┌─────────────────────────────────────────────────────────────┐
│                            HOST                             │
│                                                             │
│   Calls: update() ───────────────► [ Guest Execution Step ] │
│                                             │               │
│   Resolves: use(name) ◄─────────────────────┘               │
│   Returns: pointer to extension struct in WASM Memory       │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Binary Functions

### 2.1 Guest Exports: `update`, `setup`, `shutdown`

Every Piolho module must export the `update` function. It may optionally export `setup` and `shutdown` for worker lifecycle hooks.

```c
int32_t setup(void);    /* Optional: called once on worker startup */
int32_t update(void);   /* Mandatory: called repeatedly on each execution cycle */
int32_t shutdown(void); /* Optional: called once on worker shutdown */
```

- **WASM Type Signatures**:
  - `(func (export "setup") (result i32))`
  - `(func (export "update") (result i32))`
  - `(func (export "shutdown") (result i32))`
- **Return Codes for `update`**:
  - `0` (`UPDATE_OK`): The frame/step executed successfully. The host proceeds to the next iteration.
  - `1` (`UPDATE_EXIT`): The guest module requests a clean shutdown.
  - `<0` (`UPDATE_ERROR`): Fatal error during guest execution.

---

### 2.2 Host Imports: `use`, `hear`, `tell`

The host provides capability dispatch and rendezvous IPC imports under the `"env"` module namespace:

```c
void*   use(const char *name);
int32_t hear(const char *target, void *data, int32_t size, int32_t timeout);
int32_t tell(const char *target, const void *data, int32_t size, int32_t timeout);
```

- **WASM Import Signatures**:
  - `(import "env" "use" (func (param i32) (result i32)))`
  - `(import "env" "hear" (func (param i32 i32 i32 i32) (result i32)))`
  - `(import "env" "tell" (func (param i32 i32 i32 i32) (result i32)))`

---

## 3. Minimal ROM Example

```c
#include "piolho.h"
#include "logger.h"
#include "comm_workers.h"

static logger_t *log_ext;

int32_t setup(void) {
    log_ext = (logger_t*)use("logger");
    int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
    return 0;
}

int32_t update(void) {
    uint32_t msg = 0xCAFEBABE;
    tell("receiver", &msg, sizeof(msg), 0);
    return UPDATE_OK;
}
```
