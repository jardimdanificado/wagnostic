# Wagnostic 2.0 — Multi-ROM Worker & Rendezvous IPC Specification

## 1. Overview

Wagnostic 2.0 supports concurrent multi-ROM execution with a native synchronous rendezvous IPC model.

Every loaded ROM is a first-class, independent worker with its own:
- WebAssembly module instance
- Linear memory
- Native OS thread of execution
- Unique worker identifier and name

Communication between ROMs occurs strictly via **synchronous rendezvous**:
- There is **no persistent message queue** or mailbox on the host.
- A message exists only while two compatible operations (`wtell` and `wask`) match.
- Data transfer is performed directly between the linear memories of the communicating ROMs (`memcpy`).

---

## 2. Core IPC ABI

The host provides two core WASM imports under the `"env"` module namespace:

```c
int32_t wask(const char *target, void *data, int32_t size, int32_t timeout);
int32_t wtell(const char *target, const void *data, int32_t size, int32_t timeout);
```

### 2.1 WASM Import Signatures
- `(import "env" "wask" (func (param i32 i32 i32 i32) (result i32)))`
- `(import "env" "wtell" (func (param i32 i32 i32 i32) (result i32)))`

### 2.2 Parameters
- `target`: 32-bit byte offset pointing to a null-terminated UTF-8 string with the target worker name.
- `data`: 32-bit byte offset pointing to caller's buffer in linear memory.
- `size`: Size in bytes to transfer (`size >= 0`).
- `timeout`: Timeout in milliseconds:
  - `0`: Non-blocking (immediate match attempt).
  - `> 0`: Wait up to `timeout` milliseconds on a monotonic clock.
  - `-1`: Wait indefinitely until matched or shutdown.

### 2.3 Status / Return Codes
```c
#define WIPC_OK          1   /* Communication completed successfully */
#define WIPC_TIMEOUT     0   /* Operation timed out before match occurred */
#define WIPC_ERROR      -1   /* Generic runtime error */
#define WIPC_TARGET     -2   /* Target worker does not exist or exited */
#define WIPC_PARAM      -3   /* Invalid argument or memory out-of-bounds */
#define WIPC_SIZE       -4   /* Sender payload exceeds receiver buffer size */
#define WIPC_SHUTDOWN   -5   /* Host or worker is shutting down */
#define WIPC_STATE      -6   /* Invalid IPC state / reentrancy */
```

---

## 3. Rendezvous Semantics

A rendezvous occurs when a sender (`wtell`) and a receiver (`wask`) match:

```text
Worker "producer"                  Worker "consumer"
  wtell("consumer", data, 64)        wask("producer", buf, 64)
           \                                /
            +------ MATCH (direct memcpy) -+
```

1. **Matching Rule**: `wtell(A -> B)` matches `wask(B <- A)`.
2. **Reverse Arrival Order**: Works identically regardless of whether `wask` or `wtell` arrives first.
3. **Atomic Transfer**: Data is copied directly from sender linear memory to receiver linear memory under host synchronization lock.
4. **No Queuing**: If an operation times out or cancels, no payload remains stored in the host.

---

## 4. Multi-ROM Execution CLI

Run multiple ROM workers simultaneously on the native host:

```bash
# Run producer and consumer concurrently:
./build/wagnostic roms/ipc_producer.wasm:producer roms/ipc_consumer.wasm:consumer

# Headless execution with timeout/max frames:
./build/wagnostic --headless -n 60 roms/ipc_producer.wasm:producer roms/ipc_consumer.wasm:consumer
```
