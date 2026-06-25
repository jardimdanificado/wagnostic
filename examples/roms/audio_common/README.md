# Audio ROMs — decoders embutidos na ROM

Três ROMs demonstram decodificação de áudio onde **decoder + audio bytes**
moram dentro do próprio `.wasm` (data section do WASM linear memory). O
host só lê o ring buffer de PCM — não toca em arquivos externos.

```
examples/roms/
├── audio_common/                  # infraestrutura compartilhada
│   ├── decoders/                  # single-header C libraries
│   │   ├── dr_wav.h               # WAV (Mackron, public domain / MIT-0)
│   │   ├── dr_mp3.h               # MP3 (Mackron, public domain / MIT-0)
│   │   ├── stb_vorbis.c           # OGG Vorbis (Sean Barrett, public domain)
│   │   └── stb_vorbis.h           # shim com STB_VORBIS_HEADER_ONLY
│   ├── shim/                      # libc mínima para -nostdlib
│   │   ├── include/               # stdlib.h, string.h, limits.h,
│   │   │                          # assert.h, math.h
│   │   └── libc_shim.c            # malloc/calloc/realloc/free, memcpy/...,
│   │                              # qsort, abs — bump allocator 1MB
│   └── gen_audio.sh               # gera WAV/MP3/OGG 1.5s 22kHz e converte
│                                  # para C array via xxd
├── audio_wav/                     # dr_wav decode
├── audio_mp3/                     # dr_mp3 decode
└── audio_ogg/                     # stb_vorbis decode
```

## Fluxo de build (por ROM)

```
Python gera sine wave WAV
    ↓
ffmpeg codifica para MP3/OGG (libmp3lame, libvorbis)
    ↓
xxd -i → data/audio.h com `audio_bin[]` + `audio_bin_len`
    ↓
clang --target=wasm32 -nostdlib
   └─ inclui dr_*.h / stb_vorbis.h (header section só)
   └─ compila libc_shim.c (malloc/etc)
   └─ stb_vorbis: também compila o .c (impl section)
    ↓
.wasm final: decoder + data + ring buffer + UI, tudo no linear memory
```

## Tabela de tamanhos

| ROM      | decoder       | dados comprimidos | .wasm final |
|----------|---------------|-------------------|-------------|
| audio_wav | dr_wav 80KB   | 66 KB (PCM)       | 149 KB      |
| audio_mp3 | dr_mp3 70KB   | 19 KB (MP3 96k)   | 82 KB       |
| audio_ogg | stb_vorbis 75KB| 5 KB (OGG q4)    | 88 KB       |

Todos cabem folgadamente nos 8 MB de memória inicial.

## Dependências externas

- `ffmpeg` (com `libmp3lame` e `libvorbis`) para `gen_audio.sh`
- `python3` para o sintetizador de sine wave
- `xxd` para converter binário em C array
- `clang` com target `wasm32`

## Como o OGG resolve a math

stb_vorbis faz inverse MDCT, que precisa de `sin/cos/exp/log/pow/ldexp/frexp`.
Em wasm32, o clang rebaixa essas para imports `env.*`. O `host.c` resolve via
`m3_LinkRawFunction` chamando o glibc do host:

```c
m3_LinkRawFunction(g_module, "env", "sin",   "F(F)",  &m3_env_sin);
m3_LinkRawFunction(g_module, "env", "cos",   "F(F)",  &m3_env_cos);
// ... exp, log, pow, ldexp, frexp, fabs, floor, ceil
```

A **lógica do decoder** está embutida no ROM; a **math primitiva** vem do
host (mesmo modelo do WASI). MP3/WAV não importam math — dr_wav e dr_mp3
são integer-only na hot path.

## Inicialização

Cada ROM decodifica o áudio inteiro no primeiro `wupdate()` para um buffer
PCM estático, depois só alimenta o ring buffer nas frames seguintes. Sem
`malloc` no hot path, sem risco de underrun, loop infinito. SPACE reinicia,
ESC sai.

## Áudio sem pipocos

Três bugs sutis podem causar "pops" tipo vinyl. Os dois primeiros estão na
ROM/gen_audio; o terceiro está no **host** e foi o que mais custou pra achar.

1. **Wrap do ring buffer com sample parcial** (na ROM): se a ROM escreve
   1 byte perto do fim do buffer (quando `to_end < sample_bytes`), o host
   lê o resto da amostra no próximo chunk mas o ponteiro de leitura fica
   desalinhado. O sample resultante tem LSB de um valor e MSB de outro → click.
   **Fix**: a ROM arredonda `to_write` para baixo até um múltiplo de
   `sample_bytes` e nunca escreve uma amostra parcial.

2. **Loop discontinuity** (no gen_audio): o áudio é uma senóide que não
   termina em zero, então quando o `play_pos` volta para 0, a fase salta
   de "fim" (aleatório) para "início" (0) → pop a cada 1.5s.
   **Fix**: `gen_audio.sh` aplica 200ms de fade-out no final, garantindo que
   o último sample é 0.

3. **`max_bytes` cap errado no host** (o bug "real" que custou 3 rodadas):
   o `host_audio_callback` calcula `max_bytes = nsamples * sizeof(float)`
   (ex: 1024 × 4 = 4096) mas o buffer é `s16` (`bpp=2`), então o áudio
   consome só `nsamples * bpp = 2048` bytes por callback. O loop avança
   `r_off` por `max_bytes=4096` enquanto só lê 2048 → `r_off` anda 2× mais
   que o necessário. Após N callbacks, o áudio está lendo samples
   `N × (4096-2048) / 2 = N × 1024` samples à frente no buffer (no
   looping pcm, isso é 25 samples = meio período de 440Hz @ 22050Hz).
   Resultado: inversão de fase de 180° a cada callback = pop audível.
   **Fix**: `max_bytes = nsamples * bpp` (não `nsamples * sizeof(float)`).

4. **Race entre threads** (no host, com `-O2` em multi-core): main thread
   rodando `wupdate` e audio thread rodando `host_audio_callback` podem
   acessar `w_audio_buffer` simultaneamente. Serializado com
   `SDL_mutex` (criado no main, lock no callback inteiro + lock em volta
   de `m3_CallV(wupdate)`).
