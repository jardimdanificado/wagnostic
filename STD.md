# Piolho 2.0 — Standard Extensions Specification (`std:*`)

This document defines the official standard extension structures (`std:*`) for Piolho 2.0.

All extensions in Piolho are completely **optional** and **modular**. A host is only required to implement the extensions it can support, and a guest ROM must gracefully check if an extension pointer is non-null before accessing its memory.

---

## 1. Extension Struct Conventions

Every standard extension structure adheres to the following conventions:

1. **No Mandatory Metadata**: Structs contain only the essential fields needed for the capability without artificial headers.
2. **Endianness**: All numeric values (integers and floats) use **32-bit Little-Endian** encoding.
3. **Alignment**: Structures are aligned to 4-byte boundaries (or 8-byte for 64-bit fields).
4. **Pointers**: Any pointers within structures (`buffer`, etc.) are **32-bit byte offsets** into the guest WebAssembly module's linear memory.
5. **Field Access**:
   - Fields marked **Host (R)** are written by the host and read-only to the guest.
   - Fields marked **Host/Guest (RW)** can be negotiated or written by either party according to the extension contract.
   - Fields marked **Guest (RW)** are written by the guest module.

---

## 2. Standard Extensions Summary

| Extension Identifier | Description | Struct Size | C Header |
|---|---|:---:|---|
| `std:clock` | Monotonic ticks, tick frequency, and step delta time | 24 bytes | `clock.h` |
| `logger` | UTF-8 host console text logging buffer | 12 bytes | `logger.h` |
| `comm:tcp` | TCP peer discovery, probing, and binding | 144 bytes | `comm_tcp.h` |
| `comm:pipe` | Unix domain socket / named pipe peer discovery | 204 bytes | `comm_pipe.h` |
| `comm:ws` | WebSocket client/server discovery & binding | 208 bytes | `comm_ws.h` |
| `comm:udp` | UDP datagram probing & beacon discovery | 144 bytes | `comm_udp.h` |
| `comm:workers` | Worker sub-instances capability indicator (returns 1 or 0) | 0 bytes | `comm_workers.h` |

---

## 3. Extension Specifications

### 3.1 `std:clock`

Provides monotonic high-precision timing and step delta calculations.

- **Identifier**: `"std:clock"` (also aliases to `"clock"`)
- **Total Struct Size**: `24 bytes`
- **Header File**: `include/clock.h`

#### C Structure Definition:
```c
typedef struct {
    uint64_t ticks;      /* Monotonic tick count */
    uint64_t frequency;  /* Ticks per second */
    double   delta_time; /* Time elapsed since last step in seconds */
} clock_ext_t;
```

---

### 3.2 `logger`

Provides a simple UTF-8 text logging buffer to the host console.

- **Identifier**: `"logger"` (also aliases to `"std:logger"`)
- **Total Struct Size**: `12 bytes`
- **Header File**: `include/logger.h`

#### C Structure Definition:
```c
typedef struct {
    uint32_t buffer;      /* WASM memory pointer to UTF-8 text buffer */
    uint32_t capacity;    /* Capacity in bytes */
    uint32_t length;      /* Length of text written by ROM (host clears to 0 after printing) */
} logger_t;
```

---

### 3.3 `comm:tcp`

Enables explicit discovery and binding to remote Piolho instances over TCP. Once connected, instances communicate transparently using standard `tell` / `hear` by name.

- **Identifier**: `"comm:tcp"`
- **Total Struct Size**: `144 bytes`
- **Header File**: `include/comm_tcp.h`

#### C Structure Definition:
```c
typedef struct {
    char host[64];       /* Target host/IP or bind address */
    int32_t port;        /* TCP Port number */
    int32_t mode;        /* 0 = connect (client), 1 = listen (server) */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;  /* Number of connected peers */
    char advertised_name[32]; /* Custom local alias */
    char peer_name[32];  /* Discovered peer name */
} comm_tcp_t;
```

---

### 3.4 `comm:pipe`

Enables discovery and binding to local Piolho instances over Unix domain sockets or named pipes.

- **Identifier**: `"comm:pipe"`
- **Total Struct Size**: `204 bytes`
- **Header File**: `include/comm_pipe.h`

#### C Structure Definition:
```c
typedef struct {
    char path[128];      /* Unix socket or named pipe path */
    int32_t mode;        /* 0 = connect, 1 = listen */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;  /* Number of connected peers */
    char advertised_name[32];
    char peer_name[32];  /* Discovered peer name */
} comm_pipe_t;
```

---

### 3.5 `comm:ws`

Enables discovery and binding to remote WebSocket endpoints.

- **Identifier**: `"comm:ws"`
- **Total Struct Size**: `208 bytes`
- **Header File**: `include/comm_ws.h`

#### C Structure Definition:
```c
typedef struct {
    char url[128];       /* WebSocket URL (ws://... or wss://...) */
    int32_t port;        /* Listen port if mode == 1 */
    int32_t mode;        /* 0 = connect, 1 = listen */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;  /* Connected peer count */
    char advertised_name[32];
    char peer_name[32];  /* Discovered peer name */
} comm_ws_t;
```

---

### 3.6 `comm:udp`

Enables UDP probing, beacon discovery, and datagram binding.

- **Identifier**: `"comm:udp"`
- **Total Struct Size**: `144 bytes`
- **Header File**: `include/comm_udp.h`

#### C Structure Definition:
```c
typedef struct {
    char host[64];       /* Target host or broadcast address */
    int32_t port;        /* UDP port */
    int32_t mode;        /* 0 = probe/client, 1 = listen/server */
    int32_t status;      /* 0 = IDLE, 1 = CONNECTING, 2 = CONNECTED, -1 = ERROR */
    int32_t peer_count;  /* Connected peer count */
    char advertised_name[32];
    char peer_name[32];  /* Discovered peer name */
} comm_udp_t;
```

---

### 3.7 `comm:workers`

Capability indicator for worker threads / sub-instances in the host runtime.

- **Identifier**: `"comm:workers"` (also aliases to `"workers"`)
- **Total Struct Size**: `0 bytes` (returns integer `1` if supported, `0` otherwise)
- **Header File**: `include/comm_workers.h`

#### C Usage Example:
```c
#include "piolho.h"
#include "comm_workers.h"

int32_t setup(void) {
    int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
    if (has_workers) {
        /* Host supports multi-worker topology */
    }
    return 0;
}
```
