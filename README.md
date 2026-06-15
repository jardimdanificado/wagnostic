# Wagnostic

Wagnostic is a minimalist, platform-agnostic specification and runtime for multimedia applications.

## Memory Layout

| Offset | Hex | Name | Description |
| :--- | :--- | :--- | :--- |
| 0 | `0x000` | **Message** | Shared text buffer for Titles and Logs. |
| 128 | `0x080` | **Metadata** | width, height, bpp, scale, audio_size... |
| 172 | `0x0AC` | **Inputs** | Gamepad, Joysticks, Keyboard, Mouse. |
| 464 | `0x1D0` | **Signals** | **4 Fixed Signals** (Processed by Host). |
| 512 | `0x200` | **VRAM** | **Raw Framebuffer (Always at 512)**. |

## Quick Start

```c
#include "wagnostic.h"

void winit() {
    w_setup("Hello Wagnostic", 320, 240, 16, 2, 0);
}

void wupdate() {
    // Render something to W_FB_PTR
    w_redraw();
}
```

## Building

```bash
make            # Build Host
make -C examples # Build ROMs
./wagnostic examples/audio_test.wasm
```

## Full Specification
See [SPECIFICATION.md](./SPECIFICATION.md) for the complete technical reference.
