# Wagnostic Examples

## Structure

```
examples/                # ROMs + Makefile that builds the hosts
├── roms/                # Example ROMs
│   ├── audio_mp3
│   ├── audio_ogg
│   ├── audio_wav
│   ├── buttons_test
│   ├── display_test
│   ├── fallback_test
│   ├── full_test
│   └── input_test
└── Makefile             # builds runners/wasm3 into ./wagnostic
                         # and runners/spidermonkey into ./wagnostic-sm

runners/                 # Host runtimes (built by examples/Makefile)
├── wasm3/               # Native host (C + SDL2)
├── spidermonkey/        # Native host (C++ + SpiderMonkey + SDL2)
└── web/                 # Browser runner
```

## Building

```bash
make -C examples          # Build wasm3 host + all ROMs
make -C examples host     # Build wasm3 host only
make -C examples host-sm  # Build SpiderMonkey host only
make -C examples roms     # Build ROMs only
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
Globals that aren't exported are read as `0` by the host, so a minimal
ROM only needs `wupdate()` plus the globals it actually touches.

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
./wagnostic roms/buttons_test/buttons_test.wasm     # wasm3 host
./wagnostic-sm roms/buttons_test/buttons_test.wasm  # SpiderMonkey host
```

## Example ROMs

| ROM | Description |
|-----|-------------|
| `audio_mp3` | MP3 audio playback (dr_mp3 decoder) |
| `audio_ogg` | OGG Vorbis playback (stb_vorbis decoder) |
| `audio_wav` | WAV playback (dr_wav decoder) |
| `buttons_test` | Keyboard/mouse input grid |
| `display_test` | VRAM render at multiple bit depths |
| `fallback_test` | No globals — tests host defaults |
| `full_test` | Combined input + audio + display |
| `input_test` | Keyboard / mouse / gamepad polling |

For high-level examples (sprites, audio files, image decoding, the
wagn0 API), see [wagn0/examples/](../wagn0/examples/).
