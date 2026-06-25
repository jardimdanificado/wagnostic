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

5. **State por runner:**

   | Runner | max_bytes cap | mutex | status |
   |--------|---------------|-------|--------|
   | `native` (host.c, wasm3) | corrigido: `nsamples * bpp` | SDL_mutex | ✅ 0 abnormal jumps em WAV/MP3/OGG |
   | `native-spidermonkey` (host_sm.cpp) | já correto (lê 1 s16/iter) | SDL_mutex + math imports | ✅ WAV/MP3/OGG rodam |
   | `web` (runner.js, JS) | já correto | desnecessário (JS single-threaded) | ✅ corrigido bugs preexistentes: memory-backed globals + fallback instantiate + bounds check canvas + underrun byte math |

6. **Bugs preexistentes do web runner consertados:**

   - **Memory-backed globals — scalar vs array** (a causa raiz da maioria
     dos sintomas): o clang para wasm32 emite C globals com o `.value`
     sendo o **endereço** da variável no linear memory, não o valor.
     Para `uint32_t w_width = 320;` o endereço aponta pra um slot de 4
     bytes com o valor 320. Para `uint8_t w_vram[153600];` o endereço
     aponta pro primeiro byte do array. A regra:
       - **Scalar** (`uint32_t`, `int32_t`, etc.) → dereferenciar int32
         no endereço pra obter o valor
       - **Array** (`uint8_t arr[N]`, `char str[N]`, `Rect rs[N]`) → usar
         o endereço direto como ponteiro pros dados
     O runner agora tem `readScalar(name)` (dereferencia) e
     `readGlobal(name)` (retorna o endereço). Chamadas de escrita
     (`writeGlobal`) escrevem via memória no endereço do global, o que
     funciona mesmo em globals `mutable=0` (onde o setter `.value =`
     do browser lança TypeError). Sintomas que isso causa se mal
     implementado:
       - `Screen: 65536x65540` (lê o endereço como dimensões)
       - `createScriptProcessor: number of output channels (133900)
         exceeds maximum (32)` (lê o endereço de w_audio_channels)
       - `RangeError: Invalid typed array length: 128` no `readTitle`
         (lê os 4 primeiros bytes da string como ponteiro)

   - **OGG sem imports `env`**: stb_vorbis importa `sin/cos/exp/log/pow/
     ldexp` do módulo `env`. Sem prover esses imports o browser falha
     com `TypeError: Import #0 "env": module is not an object or
     function`. Fix: `loadRom()` agora passa um objeto `wasmImports` com
     as 6 funções de math, mapeando `Math.sin/cos/exp/log/pow` e um
     `ldexp` inline (`x * Math.pow(2, n)`). ROMs que não precisam
     desses imports (WAV/MP3) ignoram o objeto extra sem problema.

   - **`WebAssembly.instantiate(): Imports argument must be present`**:
     Polyfills ou wrappers podem exigir o segundo argumento. Fix: passar
     o `wasmImports` como imports e fazer fallback de
     `instantiateStreaming` para `WebAssembly.instantiate(bytes, ...)`.

   - **`createImageData` "Out of memory"**:
     ROM com bug ou mal carregado pode dar dimensões NaN/0. Fix: clamp em
     `resizeCanvas` para 1..8192 em ambos w e h antes de `createImageData`.

   - **Underrun byte math** (`available` em bytes, mas decrementava 1/sample):
     `available` é calculado em bytes (`writePtr - readPtr`), mas o loop
     interno decrementava por 1 por sample, não por `bpp` bytes. Para
     s16 mono (bpp=2, ch=1) detectava underrun 2× cedo, parando a leitura
     no meio de uma sample. Fix: `available -= bpp` no loop.

   - **Auto-load via `?rom=`**:
     Adicionado suporte a `?rom=name` na URL pra auto-carregar
     `name.wasm` (mesmo dir do `index.html`). Permite testar diretamente
     via Playwright/curl/links sem upload.

7. **Bugs preexistentes do SM runner consertados:**

   - **`audio_ogg` falhava no setup** com "WASM instantiation/setup JS failed".
     O setup do SM só provia `strlen` no `{env}`. OGG precisa de
     `sin/cos/exp/log/pow/ldexp/fabs/floor/ceil` (stb_vorbis MDCT em float).
     Fix: adicionado os 9 imports math no objeto JS `_imports`, mapeando
     para `Math.*` do SpiderMonkey e um `ldexp` inline (`x * Math.pow(2, n)`).
