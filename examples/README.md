# Wagnostic Examples

## Structure

```
examples/
├── roms/             # Example ROMs
│   ├── buttons_test
│   ├── fallback_test
│   └── terminal_test
└── runners/
    ├── native/       # wasm3 host
    ├── native-spidermonkey/ # SpiderMonkey host
    └── web/          # Browser host
```

## Building

```bash
make -C examples          # Build host + ROMs
make -C examples host     # Host only
make -C examples roms     # ROMs only
```

## API

A ROM exports global variables and a single function `wupdate()`:

```c
#include <stdint.h>

uint32_t w_width  = 320;
uint32_t w_height = 240;
uint32_t w_bpp    = 16;
uint8_t  w_vram[320 * 240 * 2];

int wupdate() {
    uint16_t* vram = (uint16_t*)w_vram;
    vram[w_mouse_y * w_width + w_mouse_x] = 0xF800;
    return 1;  // return 0 to quit
}
```

No `winit()`. No helper headers. Each ROM declares exactly what it needs.

## Dirty Rectangles

```c
w_dirty_count = 1;
w_dirty_rects[0] = (Rect){0, 0, w_width, w_height};
```

Or use the inline helper (define locally):
```c
static void redraw() {
    w_dirty_count = 1;
    w_dirty_rects[0] = (Rect){0, 0, (int)w_width, (int)w_height};
}
```

## Running

```bash
./wagnostic roms/buttons_test.wasm     # wasm3 host
./wagnostic-sm rom.wasm                # SpiderMonkey host
```

## Example ROMs

| ROM | Description |
|-----|-------------|
| `buttons_test` | Keyboard/mouse input grid |
| `fallback_test` | No winit — tests host defaults |
| `terminal_test` | 8bpp terminal-style rendering |

For high-level examples (sprites, audio, images, wagn0 API), see the [wagn0](https://github.com/jardimdanificado/wagn0) repository.
