# Piolho 2.0 — Multi-ROM Worker & Rendezvous IPC Specification

## 1. Overview

Piolho 2.0 supports concurrent multi-ROM execution with a native synchronous rendezvous IPC model.

Every loaded ROM is a first-class, independent worker with its own:
- WebAssembly module instance
- Linear memory
- Native execution context
- Unique worker identifier and name

Communication between ROMs occurs strictly via **synchronous rendezvous**:
- There is **no persistent message queue** or mailbox on the host.
- A message exists only while two compatible operations (`tell` and `hear`) match.
- Data transfer is performed directly between linear memories or routed across network peers.

---

## 2. Core IPC ABI

The host provides two core WASM imports under the `"env"` module namespace:

```c
int32_t hear(const char *target, void *data, int32_t size, int32_t timeout);
int32_t tell(const char *target, const void *data, int32_t size, int32_t timeout);
```

### 2.1 Parameters
- `target`: Null-terminated string with the target worker name.
  - In `tell`: Must be a valid non-empty worker name.
  - In `hear`: Can be a specific worker name, or **falsy (`NULL` / `0` / `""` / `ANY` / `HEAR_ANY`)** to accept incoming messages from **ANY** sender.
- `data`: Pointer to caller's buffer in WASM memory.
- `size`: Size in bytes to transfer (`size >= 0`).
- `timeout`: Timeout in milliseconds:
  - `0`: Non-blocking (immediate match attempt).
  - `> 0`: Wait up to `timeout` milliseconds.
  - `-1`: Wait indefinitely until matched.

### 2.2 Status / Return Codes
```c
#define IPC_OK          1   /* Communication completed successfully */
#define IPC_TIMEOUT     0   /* Operation timed out before match occurred */
#define IPC_ERROR      -1   /* Generic runtime error */
#define IPC_TARGET     -2   /* Target worker does not exist or exited */
#define IPC_PARAM      -3   /* Invalid argument or memory out-of-bounds */
#define IPC_SIZE       -4   /* Sender payload exceeds receiver buffer size */
#define IPC_SHUTDOWN   -5   /* Host or worker is shutting down */
#define IPC_STATE      -6   /* Invalid IPC state / reentrancy */
```

---

## 3. Rendezvous Semantics

A rendezvous occurs when a sender (`tell`) and a receiver (`hear`) match:

```c
// Worker "producer":
tell("consumer", &payload, sizeof(payload), 100);

// Worker "consumer":
hear("producer", &buffer, sizeof(buffer), 100);
// Ou aceita de qualquer remetente:
hear(ANY, &buffer, sizeof(buffer), 100);
```
