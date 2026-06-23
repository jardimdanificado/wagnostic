# Wagnostic Examples

## Structure

```
examples/
├── include/
│   ├── wagnostic.h   # Main API (globals + dirty rectangles)
│   └── olive.h       # Drawing library
├── roms/             # Example ROMs
├── runners/
│   ├── native/       # wasm3 host
│   ├── native-spidermonkey/ # SpiderMonkey host
│   └── web/          # Browser host
└── tools/            # Build utilities
```

## Building

```bash
make -C examples          # Build host + ROMs
make -C examples host     # Host only
make -C examples roms     # ROMs only
```

## API

```c
#define WAGNOSTIC_IMPLEMENTATION
#include "wagnostic.h"

void winit() {
    w_setup("Game", 320, 240, 16, 4, 0);
}

void wupdate() {
    // Read input
    int mx = w_mouse_x;
    int my = w_mouse_y;
    
    // Draw to w_vram
    uint16_t* vram = (uint16_t*)w_vram;
    vram[my * w_width + mx] = W_RGB565(255, 0, 0);
    
    // Mark dirty region
    w_redraw();  // or w_redraw_rect(x, y, w, h)
}
```

## Dirty Rectangles

Instead of redrawing everything, mark only what changed:

```c
w_dirty_count = 0;                    // Skip rendering
w_dirty_count = 1;                    // One rect
w_dirty_rects[0] = (Rect){x, y, w, h};
w_redraw();                           // Full screen
w_redraw_rect(x, y, w, h);           // Add rect
```

## Running

```bash
./wagnostic rom.wasm           # wasm3 host
./wagnostic-sm rom.wasm        # SpiderMonkey host
# Open runners/web/index.html  # Web host
```

## Example ROMs

| ROM | Description |
|-----|-------------|
| `buttons_test` | Input test |
| `draw_example` | Drawing primitives |
| `mouse_platformer` | Platformer with mouse |
| `images_example` | Image loading |
| `audio_example` | Audio playback |
| `roguelike_example` | Roguelike game |
| `tracker_example` | Music tracker |
| `benchmark_*` | Performance benchmarks |