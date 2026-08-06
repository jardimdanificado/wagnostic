# Wagnostic ABI

## State-Struct-Pointer API

ROMs export a single function `wupdate()` that returns a pointer (`i32` offset into WASM linear memory) to a `WagnosticState` struct. The Host dereferences that pointer and reads/writes the struct directly.

## Minimal ROM

`wupdate()` is the only required export. Return 0 to quit, or a non-zero pointer to the state struct.

```c
int wupdate() { return 0; }  // quits immediately
```

The host provides defaults for all struct fields, so a ROM with just `wupdate()` that returns a valid pointer runs immediately:

- `width = 320`, `height = 240`
- `bpp = 32`, `scale = 1`
- Input/timing: zeroed by the host each frame

## State Struct Layout

The struct is **1024 bytes** total. All offsets are from the base address returned by `wupdate()`.

```c
typedef struct { int x, y, w, h; } Rect;

typedef struct {
    uint32_t width, height, scale;
    uint32_t dirty_rects;
    int32_t mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel;
    uint8_t keys[256];
    uint32_t gamepad_buttons;
    uint32_t ticks;
    uint32_t target_fps;
    uint32_t vram_offset;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t x_bits, x_shift;
    int32_t unique;
    uint8_t reserved[676];
} State;

static struct {
    State s;
    uint8_t vram[320 * 240 * 2];
} rom;

int wupdate() {
    if (rom.s.width == 0) {
        rom.s.width = 320;
        rom.s.height = 240;
        rom.s.r_bits = 5; rom.s.r_shift = 11;
    rom.s.g_bits = 6; rom.s.g_shift = 5;
    rom.s.b_bits = 5; rom.s.b_shift = 0;
        rom.s.vram_offset = sizeof(State);
    }
    uint16_t* vram = (uint16_t*)((uint8_t*)&rom.s + rom.s.vram_offset);
    vram[rom.s.mouse_y * rom.s.width + rom.s.mouse_x] = 0xF800;
    static struct { uint32_t count; Rect rects[32]; } my_dirty;
    my_dirty.count = 1;
    my_dirty.rects[0] = (Rect){0, 0, 320, 240};
    rom.s.dirty_rects = (uint32_t)&my_dirty;
    return (int)&rom.s;
}
```

## Compilation

```bash
clang --target=wasm32 -nostdlib -O3 \
    -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
    main.c -o rom.wasm
```

Only `wupdate` needs to be exported. The `--export-all` flag is used for
convenience; in production you can export only `wupdate` and `memory`.

## Size Guarantee

The struct size is guaranteed to be exactly **1024 bytes** across all
compilers and languages. Hosts include a compile-time assertion:

```c
static_assert(sizeof(WagnosticState) == 1024, "Size mismatch");
```

If a compiler inserts unexpected padding, the build fails immediately.

## Host Extensions (`wextension`)

Custom hosts can expose non-standard host functionality to ROMs through a single imported dispatcher function:

```c
void* wextension(const char* name, void* ptr);
```

WASM import signature: `(import "env" "wextension" (func (param i32 i32) (result i32)))`.

### Semantics:
- `name`: Null-terminated string identifying the extension name/command.
- `ptr`: Untyped raw pointer (`void*` / offset in WASM linear memory) passed from ROM to host, or `NULL`.
- **Return Value**:
  - `NULL` (`0`): Returned if the requested extension is not supported by the host, or if the extension executed without returning data.
  - `void*` (non-zero offset): Pointer to return/response data.
- Reference hosts provide a default stub for `env.wextension` returning `0` (`NULL`), guaranteeing backward compatibility.

