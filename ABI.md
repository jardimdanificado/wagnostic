# Piolho — Binary ABI & Communication Specification

This document defines the official binary Application Binary Interface (ABI) of **Piolho**, including core lifecycle exports, capability negotiation (`use`), IPC rendezvous functions (`tell`, `hear`), and communication extensions (`comm:*`).

---

## 1. Core Execution Model

A Piolho module is a standard 32-bit WebAssembly (Wasm MVP) binary with linear memory.

Interaction between host and guest is governed by:
1. **Single Entry Point Export**: `update()`.
2. **Capability Import**: `use(name)`.
3. **Rendezvous IPC Imports**: `tell(target, data, size, timeout)`, `hear(target, data, size, timeout)`.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                    HOST                                     │
│                                                                             │
│   Calls: update() ───────────────────────► [ Guest Execution Step ]         │
│                                                       │                     │
│   Resolves: use(name) ◄───────────────────────────────┤                     │
│   Executes: tell(target, data, size, timeout) ◄───────┤                     │
│   Executes: hear(target, data, size, timeout) ◄───────┘                     │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Guest Module Export: `update`

Every Piolho module must export exactly one function: `update`.

```c
int32_t update(void);   /* Called on each execution step/tick */
```

- **WASM Export Signature**: `(func (export "update") (result i32))`
- **Initialization**: Standard WASM `(start)` section (executed automatically by the runtime on instantiation) or lazy first-tick initialization in `update()`.
- **Termination**: Returning `DONE` (1) signals clean shutdown. Host unloads the instance immediately.

### Return Codes for `update`:
- `0` (`UPDATE_OK`): Step completed successfully. Host continues execution.
- `1` (`UPDATE_EXIT`): Clean module exit.
- `<0` (`UPDATE_ERROR`): Fatal error in module.

---

## 3. Host Imports: `use`, `tell`, `hear`

All host functions are imported under the `"env"` module namespace.

```c
void*   use(const char *name);
int32_t tell(const char *target, const void *data, int32_t size, int32_t timeout);
int32_t hear(const char *target, void *data, int32_t size, int32_t timeout);
```

### WASM Import Signatures:
- `(import "env" "use" (func (param i32) (result i32)))`
- `(import "env" "tell" (func (param i32 i32 i32 i32) (result i32)))`
- `(import "env" "hear" (func (param i32 i32 i32 i32) (result i32)))`

---

## 4. Synchronous Rendezvous IPC (`tell` & `hear`)

Inter-module and cross-node communication in Piolho is based on **synchronous rendezvous**. Data transfers directly between linear memories or across network transports when matching `tell` and `hear` calls meet.

### 4.1 Parameters:

#### `tell(target, data, size, timeout)`
- `target`: Null-terminated string identifying the destination worker or peer name.
- `data`: Pointer to source payload buffer in caller's WASM memory.
- `size`: Payload size in bytes (`size >= 0`).
- `timeout`: Timeout in milliseconds (`0` = non-blocking, `>0` = wait up to $N$ ms, `-1` = wait indefinitely).

#### `hear(target, data, size, timeout)`
- `target`: Specific sender name, or `NULL` / `""` / `ANY` (`HEAR_ANY`) to accept data from any sender.
- `data`: Pointer to destination buffer in caller's WASM memory.
- `size`: Buffer capacity in bytes.
- `timeout`: Timeout in milliseconds (`0` = non-blocking, `>0` = wait up to $N$ ms, `-1` = wait indefinitely).

### 4.2 Unified Status & Return Codes:

Both `update()` and IPC operations (`tell`, `hear`) share a single, unified status code definition:

```c
#define OK               0   /* Success */
#define DONE             1   /* Finished / Clean Exit / End of stream */
#define EXIT             1   /* Alias for DONE */
#define TIMEOUT          2   /* Timed out before match/rendezvous */

#define ERROR           -1   /* Generic error */
#define ERROR_TARGET    -2   /* Target peer/worker not found */
#define ERROR_PARAM     -3   /* Invalid parameter or memory bounds */
#define ERROR_SIZE      -4   /* Message payload exceeds buffer size */
#define ERROR_SHUTDOWN  -5   /* Host or worker is shutting down */
#define ERROR_STATE     -6   /* Invalid runtime state / reentrancy */
```

---

## 5. Communication Extensions (`comm:*`)

Communication extensions enable active and passive discovery across processes and networks. Once connected, messaging between discovered peers is performed transparently through standard `tell` and `hear`.

### 5.1 `comm:tcp`
TCP client connection or server binding.
- **Header**: `include/comm_tcp.h`
- **Size**: 144 bytes

```c
typedef struct {
    char host[64];            /* Remote host IP or local bind address */
    int32_t port;             /* TCP Port */
    int32_t mode;             /* 0 = connect (client), 1 = listen (server) */
    int32_t status;           /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;       /* Active peer count */
    char advertised_name[32]; /* Local advertised alias */
    char peer_name[32];       /* Discovered peer name */
} comm_tcp_t;
```

---

### 5.2 `comm:pipe`
Unix domain socket / named pipe local IPC.
- **Header**: `include/comm_pipe.h`
- **Size**: 204 bytes

```c
typedef struct {
    char path[128];           /* Socket file path */
    int32_t mode;             /* 0 = connect, 1 = listen */
    int32_t status;           /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;       /* Active peer count */
    char advertised_name[32]; /* Local advertised alias */
    char peer_name[32];       /* Discovered peer name */
} comm_pipe_t;
```

---

### 5.3 `comm:ws`
WebSocket network connection and endpoint discovery.
- **Header**: `include/comm_ws.h`
- **Size**: 208 bytes

```c
typedef struct {
    char url[128];            /* ws:// or wss:// URL */
    int32_t port;             /* Listen port (when mode == 1) */
    int32_t mode;             /* 0 = connect, 1 = listen */
    int32_t status;           /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;       /* Active peer count */
    char advertised_name[32]; /* Local advertised alias */
    char peer_name[32];       /* Discovered peer name */
} comm_ws_t;
```

---

### 5.4 `comm:udp`
UDP datagram discovery and broadcast beacons.
- **Header**: `include/comm_udp.h`
- **Size**: 144 bytes

```c
typedef struct {
    char host[64];            /* Target host or broadcast address */
    int32_t port;             /* UDP Port */
    int32_t mode;             /* 0 = probe/client, 1 = listen/server */
    int32_t status;           /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;       /* Active peer count */
    char advertised_name[32]; /* Local advertised alias */
    char peer_name[32];       /* Discovered peer name */
} comm_udp_t;
```

---

### 5.5 `comm:workers`
Indicator of multi-worker / multi-instance host capability.
- **Header**: `include/comm_workers.h`
- **Size**: 0 bytes (returns integer `1` if supported, `0` otherwise)

```c
int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
```

---

## 6. System Extensions (`clock`, `logger`)

### 6.1 `clock`
- **Header**: `include/clock.h` (24 bytes)
```c
typedef struct {
    uint64_t ticks;      /* Monotonic tick counter */
    uint64_t frequency;  /* Ticks per second */
    float    delta;      /* Elapsed seconds since last step */
} clock_ext_t;
```

### 6.2 `logger`
- **Header**: `include/logger.h` (12 bytes)
```c
typedef struct {
    uint32_t buffer;    /* WASM byte offset to UTF-8 text */
    uint32_t capacity;  /* Buffer size in bytes */
    uint32_t length;    /* Bytes written by ROM */
} logger_t;
```
