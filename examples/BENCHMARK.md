# Wagnostic Benchmark Suite

Suite de benchmarks que levam o wagnostic ao limite em diferentes áreas.

## Benchmarks

### benchmark_cpu.wasm
**Stress test: Cálculos de ponto flutuante**
- Mandelbrot fractal 512x512 @ 32bpp
- 1000 iterações por pixel
- ~262M operações de ponto flutuante por frame
- **Resultado típico:** 0.3 FPS (3.7s/frame)

### benchmark_vram.wasm
**Stress test: Bandwidth de memória**
- Resolução 4K (3840x2160) @ 32bpp
- Preenche 32MB de VRAM por frame
- Testa bandwidth de upload para o host
- **Resultado típico:** 8 FPS, 260 MB/s

### benchmark_audio.wasm
**Stress test: Processamento de áudio**
- 64 oscillators com frequências consonantes (harmônicos musicais)
- 48kHz, stereo, 32-bit float
- 1MB ring buffer
- 2048 samples stereo por frame
- Envelope ADSR para evitar clicks
- Volume limitado a 30% com soft clip
- **Resultado típico:** 130 FPS

### benchmark_particles.wasm
**Stress test: Física + renderização**
- 50.000 partículas com física (gravidade, colisão, vida)
- Resolução 1280x720 @ 32bpp
- Combina CPU (física) + VRAM (renderização)
- **Resultado típico:** 150 FPS, 520 MB/s bandwidth

### benchmark_all.wasm
**Stress test: Todos os sistemas**
- 1280x720 @ 32bpp com gradiente animado
- 5.000 partículas com física
- 16 oscillators de áudio
- 1MB audio buffer
- **Resultado típico:** 110 FPS

## Uso

```bash
# Compilar e rodar todos os benchmarks (100 frames cada)
./benchmark.sh

# Rodar com número customizado de frames
./benchmark.sh 50

# Compilar apenas os ROMs
make benchmarks

# Compilar apenas o host de benchmark
make host-bench

# Rodar um benchmark individual
./wagnostic-bench benchmark_cpu.wasm 100
```

## Host de Benchmark

O `wagnostic-bench` é um host headless (sem SDL/display) que:
- Carrega o WASM
- Chama `winit()` uma vez
- Chama `wupdate()` N vezes
- Mede tempo total, tempo por frame, FPS
- Reporta bandwidth de VRAM e samples de áudio

Não requer display, audio device, ou qualquer periférico.

## Métricas

- **Avg frame time**: Tempo médio por frame (ms)
- **FPS**: Frames por segundo
- **MP/s**: Megapixels processados por segundo
- **BW (MB/s)**: Bandwidth de VRAM (upload para o host)

## Interpretação

- **CPU bound**: benchmark_cpu (0.3 FPS) - wasm3 interpretando cálculos pesados
- **Memory bound**: benchmark_vram (8 FPS) - bandwidth de memória limita
- **Balanced**: benchmark_particles (150 FPS) - física + renderização
- **Audio bound**: benchmark_audio (75 FPS) - processamento de áudio
- **Stress total**: benchmark_all (110 FPS) - combina tudo

## Notas

- Os benchmarks usam aproximação polinomial de seno (Bhaskara I) para evitar dependência de libc
- O wasm3 interpreta WASM, então performance é ~10-100x mais lenta que native
- Resoluções altas (4K) alocam muita memória (64MB initial-memory)
- O benchmark_all foi ajustado para ser pesado mas executável (<10s/frame)
- **Áudio seguro**: Volume limitado a 20-30%, envelope ADSR para evitar clicks, soft clip para prevenir distorção, frequências consonantes (harmônicos musicais)
