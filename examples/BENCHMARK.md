# Wagnostic Benchmark Suite

A suite of benchmarks that push Wagnostic to its limits across different areas: CPU, memory bandwidth, audio processing, particle physics, and combined stress testing.

## Benchmarks

### benchmark_cpu.wasm
**Stress test: Floating point calculations**
- Mandelbrot fractal 512x512 @ 32bpp
- 1000 iterations per pixel
- ~262M floating point operations per frame
- **Typical result:** 0.3 FPS (~3.7s/frame on wasm3)

### benchmark_vram.wasm
**Stress test: Memory bandwidth**
- 4K resolution (3840x2160) @ 32bpp
- 32MB VRAM fill per frame
- Tests host upload bandwidth
- **Typical result:** 8 FPS, 260 MB/s (wasm3)

### benchmark_audio.wasm
**Stress test: Audio processing**
- 64 oscillators with consonant frequencies (musical harmonics)
- 48kHz, stereo, 32-bit float
- 1MB ring buffer
- 2048 stereo samples per frame
- ADSR envelope (click-free), soft-clip, 30% max volume
- **Typical result:** 130 FPS (wasm3)

### benchmark_particles.wasm
**Stress test: Physics + rendering**
- 50,000 particles with physics (gravity, collision, lifetime)
- 1280x720 @ 32bpp resolution
- Combines CPU (physics) + VRAM (rendering)
- **Typical result:** 150 FPS, 520 MB/s bandwidth (wasm3)

### benchmark_all.wasm
**Stress test: All systems combined**
- 1280x720 @ 32bpp animated gradient
- 5,000 particles with physics
- 16 audio oscillators
- 1MB audio buffer
- **Typical result:** 110 FPS (wasm3)

## Usage

```bash
# Build and run all benchmarks (100 frames each)
./benchmark.sh

# Custom frame count
./benchmark.sh 50

# Build only ROMs
make benchmarks

# Build benchmark hosts
make host-bench          # wasm3 headless (native-wasm3)
make host-bench-native   # Wasmtime JIT + GPU (PBO + triple buffer)
make host-bench-v8       # V8 TurboFan JIT headless

# Build interactive hosts
make host                # wasm3 CPU (wagnostic)
make host-gpu            # wasm3 GPU (wagnostic-gpu)

# Run individual benchmarks
./wagnostic-bench benchmark_cpu.wasm 100
./wagnostic-bench-native benchmark_cpu.wasm 100
./wagnostic-bench-v8 benchmark_cpu.wasm 100
```

## Runners

| Runner | Engine | Rendering | Binary |
|---|---|---|---|
| **native-wasm3** | wasm3 (interpreter) | OpenGL 1.2 (CPU pixel conversion) | `wagnostic` |
| **native** | Wasmtime (Cranelift JIT) | OpenGL 3.3 Core (GPU shader) + PBO + triple buffer | `wagnostic-bench-native` |
| **V8** | V8 (TurboFan JIT) | Headless (no GPU) | `wagnostic-bench-v8` |
| **LÖVE** | wasm3 (FFI) | LÖVE2D (OpenGL) | `love /tmp/love-bench rom.wasm` |

## Results (5 frames each)

| Benchmark | native-wasm3 | native | V8 | LÖVE |
|---|---|---|---|---|
| **CPU** (Mandelbrot) | 3696ms | **477ms** (7.8x) | **498ms** (7.4x) | 3815ms (1.0x) |
| **VRAM** (4K fill) | 117ms | **52ms** (2.3x) | **19ms** (6.2x) | 133ms (0.9x) |
| **Audio** (64 osc) | 7.9ms | **2.6ms** (3.1x) | **1.1ms** (7.4x) | 7.9ms (1.0x) |
| **Particles** (50K) | 7.1ms | **4.4ms** (1.6x) | **1.0ms** (7.4x) | 7.2ms (1.0x) |
| **Stress All** | 9.6ms | **4.6ms** (2.1x) | **1.4ms** (6.8x) | 9.2ms (1.1x) |

### Analysis

- **V8 (TurboFan JIT)** is the fastest engine, 6-7x over wasm3, being headless (no GPU overhead)
- **native (Wasmtime + GPU)** is 2-7x over wasm3 but includes full GPU pipeline (texture upload, shader, PBO, triple buffer)
- **LÖVE** matches wasm3's performance (same engine underneath), confirming the Lua FFI overhead is negligible

## Notes

- Benchmarks use Bhaskara I sine approximation (polynomial) to avoid libc dependency
- wasm3 is a pure interpreter (~10-100x slower than native code)
- Wasmtime Cranelift JIT compiles WASM to native x86-64
- V8 TurboFan JIT compiles to optimized machine code
- High-resolution ROMs (4K) allocate 64MB initial memory
- CPU headless benchmark is `wagnostic-bench` (wasm3), `wagnostic-bench-v8` (V8)
- GPU benchmarks use `wagnostic-bench-native` (Wasmtime + triple buffer + PBO + shader)
- **Safe audio**: volume capped at 20-30%, ADSR envelope (no clicks), soft-clip, consonant musical frequencies
