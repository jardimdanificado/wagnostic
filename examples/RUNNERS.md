# Wagnostic Runners

## Runner Summary

| Runner | Engine | Render | Size | Features |
|--------|--------|--------|------|----------|
| `wagnostic` | wasm3 | SDL2 software | 185KB | Portable, no OpenGL |
| `wagnostic-gpu` | wasm3 | OpenGL 3.3 | 190KB | GPU shaders |
| `wagnostic-wasmtime` | Wasmtime JIT | OpenGL 3.3 + PBO | 29KB | High performance |
| `wagnostic-v8` | V8 13 JIT | OpenGL 3.3 + PBO | 40MB | Maximum performance |

## Benchmarks

| Binary | Engine | Size |
|--------|--------|------|
| `wagnostic-bench` | wasm3 | 179KB |
| `wagnostic-bench-wasmtime` | Wasmtime JIT | 27KB |
| `wagnostic-bench-v8` | V8 13 JIT | 40MB |
| `wagnostic-bench-wamr` | WAMR | 408KB |

## Compilation

```bash
# Runners
make host              # wasm3 (portable)
make host-gpu          # wasm3 + OpenGL
make host-wasmtime     # Wasmtime + PBO + triple buffer
make host-v8           # V8 + PBO + triple buffer

# Benchmarks
make host-bench        # wasm3 benchmark
make host-bench-wasmtime  # Wasmtime benchmark
make host-bench-v8     # V8 benchmark
```

## Execution

```bash
# Run ROM
./wagnostic rom.wasm
./wagnostic-wasmtime rom.wasm
./wagnostic-v8 rom.wasm

# Run benchmark
./benchmark.sh [num_frames]
```

## Technical Features

### Triple Buffer
- 3 textures in round-robin
- Allows CPU/GPU overlap

### PBO (Pixel Buffer Objects)
- 2 alternating PBOs
- Async upload via DMA
- `glMapBufferRange` with `GL_MAP_INVALIDATE_BUFFER_BIT`

### Shaders (OpenGL runners)
- RGB332/RGB565/RGBA decode in fragment shader
- Native textures (R8, RG8, RGBA8)

## Portability

| Platform | wasm3 | Wasmtime | V8 |
|----------|-------|----------|-----|
| Linux x64 | ✓ | ✓ | ✓ |
| Linux ARM | ✓ | ✓ | - |
| Windows | ✓ | ✓ | ✓ |
| macOS | ✓ | ✓ | ✓ |
| Raspberry Pi | ✓ | ✓ | - |
| Headless | ✓ | - | - |

## Dependencies

### wasm3
- SDL2
- C99 compiler

### Wasmtime
- SDL2, OpenGL 3.3+
- libwasmtime

### V8
- SDL2, OpenGL 3.3+
- libv8_monolith, icudtl.dat
- C++20 compiler
