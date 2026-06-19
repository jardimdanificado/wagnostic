# Wagnostic Benchmarks

## Available Runners

| Runner | Engine | Render | Size | Flags |
|--------|--------|--------|------|-------|
| `wagnostic` | wasm3 | SDL2 software + triple buffer | ~185KB | Portable, no OpenGL |
| `wagnostic-wasmtime` | Wasmtime JIT | OpenGL 3.3 + PBO + triple buffer | ~29KB | High performance |
| `wagnostic-v8` | V8 13 JIT | OpenGL 3.3 + PBO + triple buffer | ~41MB | Maximum performance |

## Runner Features

### wasm3 (Portable)
- **Engine:** wasm3 interpreter
- **Render:** SDL2 software rendering
- **Triple Buffer:** 3 SDL textures
- **Conversion:** CPU (RGB332/RGB565 → RGBA)
- **Dependencies:** SDL2 only
- **Use case:** Maximum portability (headless, embedded, etc)

### Wasmtime (Performance)
- **Engine:** Wasmtime JIT
- **Render:** OpenGL 3.3 core
- **Triple Buffer:** 3 OpenGL textures
- **PBO:** 2 Pixel Buffer Objects for async upload
- **Shaders:** RGB332/RGB565 decode in fragment shader
- **Dependencies:** SDL2, OpenGL, libwasmtime

### V8 (Maximum Performance)
- **Engine:** V8 13 JIT
- **Render:** OpenGL 3.3 core
- **Triple Buffer:** 3 OpenGL textures
- **PBO:** 2 Pixel Buffer Objects for async upload
- **Shaders:** RGB332/RGB565 decode in fragment shader
- **Dependencies:** SDL2, OpenGL, libv8_monolith, icudtl.dat

## Compilation

```bash
# Portable runner (wasm3 + SDL)
make host

# Wasmtime runner (JIT + OpenGL)
make host-wasmtime

# V8 runner (JIT + OpenGL)
make host-v8
```

## Execution

```bash
# Run ROM
./wagnostic rom.wasm
./wagnostic-wasmtime rom.wasm
./wagnostic-v8 rom.wasm
```

## Benchmark

```bash
# Run full benchmark suite
./benchmark.sh [num_frames]

# Example: 500 frames
./benchmark.sh 500
```

The benchmark runs all available runners and generates a comparison table with:
- Avg frame time (ms)
- FPS
- Megapixels/second
- VRAM bandwidth (MB/s)
- Speedup relative to wasm3

## Available Benchmarks

| ROM | Description |
|-----|-------------|
| `benchmark_cpu.wasm` | CPU-intensive calculations |
| `benchmark_vram.wasm` | VRAM transfer |
| `benchmark_audio.wasm` | Audio processing |
| `benchmark_particles.wasm` | Particle system |
| `benchmark_all.wasm` | Combined test suite |

## Render Architecture

### Triple Buffer
```
Frame N:   memcpy VRAM → PBO[N%2]
           upload PBO[(N-1)%2] → texture[N%3]
           render texture[(N-2)%3]
```

### PBO (Pixel Buffer Objects)
- 2 alternating PBOs
- Async upload (CPU and GPU in parallel)
- `glMapBufferRange` with `GL_MAP_INVALIDATE_BUFFER_BIT`

### Shaders (Wasmtime/V8)
```glsl
// Fragment shader RGB332 decode
uint raw = uint(texelFetch(vram, pix, 0).r * 255.0);
float r = float((raw >> 5) & 0x07u) / 7.0;
float g = float((raw >> 2) & 0x07u) / 7.0;
float b = float(raw & 0x03u) / 3.0;
```

## Portability

| Platform | wasm3 | Wasmtime | V8 |
|----------|-------|----------|-----|
| Linux x64 | ✓ | ✓ | ✓ |
| Linux ARM | ✓ | ✓ | - |
| Windows | ✓ | ✓ | ✓ |
| macOS | ✓ | ✓ | ✓ |
| Raspberry Pi | ✓ | ✓ | - |
| Headless | ✓ | - | - |
| Browser | - | - | - |

## Dependencies

### wasm3
- SDL2
- C99 compiler

### Wasmtime
- SDL2
- OpenGL 3.3+
- libwasmtime (runners/lib/wasmtime/)

### V8
- SDL2
- OpenGL 3.3+
- libv8_monolith (runners/native-v8/v8_Linux_x64/)
- icudtl.dat (runtime)
- C++20 compiler
