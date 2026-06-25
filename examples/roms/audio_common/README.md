# Audio ROMs — decoders embedded in the ROM

Three ROMs demonstrate audio decoding where **decoder + audio bytes** live inside the `.wasm` itself (WASM linear memory data section). The host only reads the PCM ring buffer — it doesn't touch any external files.

```
examples/roms/
├── audio_common/                  # shared infrastructure
│   ├── decoders/                  # single-header C libraries
│   │   ├── dr_wav.h               # WAV (Mackron, public domain / MIT-0)
│   │   ├── dr_mp3.h               # MP3 (Mackron, public domain / MIT-0)
│   │   ├── stb_vorbis.c           # OGG Vorbis (Sean Barrett, public domain)
│   │   └── stb_vorbis.h           # shim with STB_VORBIS_HEADER_ONLY
│   ├── shim/                      # minimal libc for -nostdlib
│   │   ├── include/               # stdlib.h, string.h, limits.h,
│   │   │                          # assert.h, math.h
│   │   └── libc_shim.c            # malloc/calloc/realloc/free, memcpy/...,
│   │                              # qsort, abs — bump allocator 1MB
│   └── gen_audio.sh               # generates WAV/MP3/OGG 1.5s 22kHz and converts
│                                  # to C array via xxd
├── audio_wav/                     # dr_wav decode
├── audio_mp3/                     # dr_mp3 decode
└── audio_ogg/                     # stb_vorbis decode
```

## Build flow (per ROM)

```
Python generates sine wave WAV
    ↓
ffmpeg encodes to MP3/OGG (libmp3lame, libvorbis)
    ↓
xxd -i → data/audio.h with `audio_bin[]` + `audio_bin_len`
    ↓
clang --target=wasm32 -nostdlib
   └─ includes dr_*.h / stb_vorbis.h (header section only)
   └─ compiles libc_shim.c (malloc/etc)
   └─ stb_vorbis: also compiles the .c (impl section)
    ↓
.wasm final: decoder + data + ring buffer + UI, all in linear memory
```

## Size table

| ROM      | decoder       | compressed data | .wasm final |
|----------|---------------|-----------------|-------------|
| audio_wav | dr_wav 80KB   | 66 KB (PCM)     | 149 KB      |
| audio_mp3 | dr_mp3 70KB   | 19 KB (MP3 96k) | 82 KB       |
| audio_ogg | stb_vorbis 75KB| 5 KB (OGG q4)  | 88 KB       |

All fit comfortably within the 8 MB initial memory.

## External dependencies

- `ffmpeg` (with `libmp3lame` and `libvorbis`) for `gen_audio.sh`
- `python3` for the sine wave synthesizer
- `xxd` to convert binary to C array
- `clang` with target `wasm32`

## How OGG resolves math

stb_vorbis does inverse MDCT, which requires `sin/cos/exp/log/pow/ldexp/frexp`.
In wasm32, clang lowers these to `env.*` imports. The `host.c` resolves them via
`m3_LinkRawFunction` calling the host's glibc:

```c
m3_LinkRawFunction(g_module, "env", "sin",   "F(F)",  &m3_env_sin);
m3_LinkRawFunction(g_module, "env", "cos",   "F(F)",  &m3_env_cos);
// ... exp, log, pow, ldexp, frexp, fabs, floor, ceil
```

The **decoder logic** is embedded in the ROM; the **primitive math** comes from
the host (same model as WASI). MP3/WAV don't import math — dr_wav and dr_mp3
are integer-only on the hot path.

## Initialization

Each ROM decodes the entire audio on the first `wupdate()` into a static
PCM buffer, then just feeds the ring buffer on subsequent frames. No
`malloc` on the hot path, no risk of underrun, infinite loop. SPACE restarts,
ESC exits.

## Audio without pops

Three subtle bugs can cause vinyl-like "pops". The first two are in the
ROM/gen_audio; the third is in the **host** and was the hardest to track down.

1. **Ring buffer wrap with partial sample** (in ROM): if the ROM writes
   1 byte near the end of the buffer (when `to_end < sample_bytes`), the host
   reads the rest of the sample in the next chunk but the read pointer becomes
   misaligned. The resulting sample has the LSB of one value and MSB of
   another → click.
   **Fix**: the ROM rounds `to_write` down to a multiple of
   `sample_bytes` and never writes a partial sample.

2. **Loop discontinuity** (in gen_audio): the audio is a sine wave that doesn't
   end at zero, so when `play_pos` wraps back to 0, the phase jumps from
   "end" (arbitrary) to "start" (0) → pop every 1.5s.
   **Fix**: `gen_audio.sh` applies 200ms fade-out at the end, ensuring the
   last sample is 0.

3. **Wrong `max_bytes` cap in host** (the "real" bug that cost 3 rounds):
   `host_audio_callback` calculates `max_bytes = nsamples * sizeof(float)`
   (e.g. 1024 × 4 = 4096) but the buffer is `s16` (`bpp=2`), so audio
   only consumes `nsamples * bpp = 2048` bytes per callback. The loop advances
   `r_off` by `max_bytes=4096` while only reading 2048 → `r_off` advances 2×
   more than necessary. After N callbacks, audio is reading samples
   `N × (4096-2048) / 2 = N × 1024` ahead in the buffer (in looping pcm,
   that's 25 samples = half period of 440Hz @ 22050Hz).
   Result: 180° phase inversion every callback = audible pop.
   **Fix**: `max_bytes = nsamples * bpp` (not `nsamples * sizeof(float)`).

4. **Thread race** (in host, with `-O2` on multi-core): main thread running
   `wupdate` and audio thread running `host_audio_callback` can access
   `w_audio_buffer` simultaneously. Serialized with `SDL_mutex` (created in
   main, lock on entire callback + lock around `m3_CallV(wupdate)`).

5. **State per runner:**

   | Runner | max_bytes cap | mutex | status |
   |--------|---------------|-------|--------|
   | `native` (host.c, wasm3) | fixed: `nsamples * bpp` | SDL_mutex | ✅ 0 abnormal jumps in WAV/MP3/OGG |
   | `native-spidermonkey` (host_sm.cpp) | already correct (reads 1 s16/iter) | SDL_mutex + math imports | ✅ WAV/MP3/OGG run |
   | `web` (runner.js, JS) | already correct | unnecessary (JS single-threaded) | ✅ fixed preexisting bugs: memory-backed globals + fallback instantiate + bounds check canvas + underrun byte math |

6. **Preexisting web runner bugs fixed:**

   - **Memory-backed globals — scalar vs array** (root cause of most symptoms):
     clang for wasm32 emits C globals where `.value` is the **address** of the
     variable in linear memory, not the value.
     For `uint32_t w_width = 320;` the address points to a 4-byte slot with
     value 320. For `uint8_t w_vram[153600];` the address points to the first
     byte of the array. The rule:
       - **Scalar** (`uint32_t`, `int32_t`, etc.) → dereference int32
         at the address to get the value
       - **Array** (`uint8_t arr[N]`, `char str[N]`, `Rect rs[N]`) → use
         the address directly as a pointer to the data
     The runner now has `readScalar(name)` (dereferences) and
     `readGlobal(name)` (returns the address). Write calls (`writeGlobal`)
     write via memory at the global's address, which works even on
     `mutable=0` globals (where the browser's `.value =` setter throws
     TypeError). Symptoms when implemented incorrectly:
       - `Screen: 65536x65540` (reads address as dimensions)
       - `createScriptProcessor: number of output channels (133900)
         exceeds maximum (32)` (reads w_audio_channels address)
       - `RangeError: Invalid typed array length: 128` in `readTitle`
         (reads first 4 bytes of string as pointer)

   - **OGG without `env` imports**: stb_vorbis imports `sin/cos/exp/log/pow/
     ldexp` from the `env` module. Without providing these imports the browser
     fails with `TypeError: Import #0 "env": module is not an object or
     function`. Fix: `loadRom()` now passes a `wasmImports` object with
     6 math functions, mapping `Math.sin/cos/exp/log/pow` and an inline
     `ldexp` (`x * Math.pow(2, n)`). ROMs that don't need these imports
     (WAV/MP3) ignore the extra object without issue.

   - **`WebAssembly.instantiate(): Imports argument must be present`**:
     Polyfills or wrappers may require the second argument. Fix: pass
     `wasmImports` as imports and fall back from `instantiateStreaming`
     to `WebAssembly.instantiate(bytes, ...)`.

   - **`createImageData` "Out of memory"**:
     A bugged or badly loaded ROM may produce NaN/0 dimensions. Fix: clamp in
     `resizeCanvas` to 1..8192 for both w and h before `createImageData`.

   - **Underrun byte math** (`available` in bytes, but decremented 1/sample):
     `available` is calculated in bytes (`writePtr - readPtr`), but the inner
     loop decremented by 1 per sample, not by `bpp` bytes. For s16 mono
     (bpp=2, ch=1) it detected underrun 2× early, stopping the read in the
     middle of a sample. Fix: `available -= bpp` in the loop.

   - **Auto-load via `?rom=`**:
     Added support for `?rom=name` in the URL to auto-load `name.wasm`
     (same dir as `index.html`). Allows testing directly via Playwright/curl/
     links without upload.

7. **Preexisting SpiderMonkey runner bugs fixed:**

   - **`audio_ogg` failed setup** with "WASM instantiation/setup JS failed".
     The SM setup only provided `strlen` in `{env}`. OGG needs
     `sin/cos/exp/log/pow/ldexp/fabs/floor/ceil` (stb_vorbis MDCT in float).
     Fix: added 9 math imports to the JS `_imports` object, mapping to
     SpiderMonkey's `Math.*` and an inline `ldexp` (`x * Math.pow(2, n)`).
