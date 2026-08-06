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
- `vram_offset = sizeof(State)` (44)

## State Struct Layout

The struct is **44 bytes** total. All offsets are from the base address returned by `wupdate()`.

```c
typedef struct {
    uint32_t width, height;
    uint32_t r_bits, r_shift;
    uint32_t g_bits, g_shift;
    uint32_t b_bits, b_shift;
    uint32_t a_bits, a_shift;
    uint32_t vram_offset;
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
    return (int)&rom.s;
}
```

### Offset Table

| Field | Offset (Bytes) | Size | Description |
| :--- | :--- | :--- | :--- |
| `width` | 0 | 4 | Framebuffer width |
| `height` | 4 | 4 | Framebuffer height |
| `r_bits` / `r_shift` | 8 / 12 | 8 | Red channel bitmask width and shift |
| `g_bits` / `g_shift` | 16 / 20 | 8 | Green channel bitmask width and shift |
| `b_bits` / `b_shift` | 24 / 28 | 8 | Blue channel bitmask width and shift |
| `a_bits` / `a_shift` | 32 / 36 | 8 | Alpha channel bitmask width and shift |
| `vram_offset` | 40 | 4 | Byte offset to VRAM relative to State pointer |

## Size Guarantee

The struct size is guaranteed to be exactly **44 bytes** across all compilers and languages. Hosts include a compile-time assertion:

```c
static_assert(sizeof(WagnosticState) == 44, "Size mismatch");
```

If a compiler inserts unexpected padding, the build fails immediately.

## Host Extensions (`wextension`)

Hosts expose optional host functionality and modular input peripherals to ROMs through a single imported dispatcher function:

```c
void* wextension(const char* name, void* ptr);
```

WASM import signature: `(import "env" "wextension" (func (param i32 i32) (result i32)))`.

### Standard Extensions (`std:*`)

- **`std:keyboard`**: Registers/returns `uint8_t*` pointer to a 256-byte array of USB HID keyboard scancodes (`0` = released, `1` = pressed).
- **`std:mouse`**: Registers/returns pointer to `struct { int32_t x, y; uint32_t buttons; int32_t wheel; }` (16 bytes).
- **`std:gamepad`**: Registers/returns `uint32_t*` pointer to digital gamepad buttons mask.
