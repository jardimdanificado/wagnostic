#!/usr/bin/env bash
# Wagnostic Benchmark Runner
# Compila e executa todos os benchmarks, mostrando tempos no console
#
# Usage: ./benchmark.sh [num_frames]
#   num_frames: número de frames por benchmark (default: 100)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

FRAMES="${1:-100}"
BENCH_HOST="./wagnostic-bench"

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║          WAGNOSTIC BENCHMARK SUITE              ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================
# Step 1: Compilar host de benchmark
# ============================================================
echo -e "${CYAN}[1/3] Compilando host de benchmark...${NC}"
make host-bench 2>&1 | tail -1
if [ ! -f "$BENCH_HOST" ]; then
    echo -e "${RED}ERRO: Falha ao compilar wagnostic-bench${NC}"
    exit 1
fi
echo -e "${GREEN}  OK: $BENCH_HOST${NC}"
echo ""

# ============================================================
# Step 2: Compilar ROMs de benchmark
# ============================================================
echo -e "${CYAN}[2/3] Compilando ROMs de benchmark...${NC}"
make benchmarks 2>&1 | grep -E "^(clang|make)" | while read -r line; do
    echo "  $line"
done

BENCHMARKS=(
    "benchmark_cpu.wasm"
    "benchmark_vram.wasm"
    "benchmark_audio.wasm"
    "benchmark_particles.wasm"
    "benchmark_all.wasm"
)

for bm in "${BENCHMARKS[@]}"; do
    if [ ! -f "$bm" ]; then
        echo -e "${RED}ERRO: Falha ao compilar $bm${NC}"
        exit 1
    fi
    SIZE=$(du -h "$bm" | cut -f1)
    echo -e "${GREEN}  OK: $bm ($SIZE)${NC}"
done
echo ""

# ============================================================
# Step 3: Executar benchmarks
# ============================================================
echo -e "${CYAN}[3/3] Executando benchmarks ($FRAMES frames cada)...${NC}"
echo ""

RESULTS_FILE=$(mktemp /tmp/wagnostic_bench.XXXXXX)

for bm in "${BENCHMARKS[@]}"; do
    echo -e "${YELLOW}────────────────────────────────────────────────────${NC}"
    echo -e "${BOLD}  Running: $bm${NC}"
    echo -e "${YELLOW}────────────────────────────────────────────────────${NC}"
    
    # Executar benchmark e capturar output
    OUTPUT=$("$BENCH_HOST" "$bm" "$FRAMES" 2>&1)
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -ne 0 ]; then
        echo -e "${RED}  FAILED (exit code $EXIT_CODE)${NC}"
        echo "$OUTPUT"
        echo ""
        continue
    fi
    
    # Exibir output do benchmark
    echo "$OUTPUT" | while IFS= read -r line; do
        echo "  $line"
    done
    echo ""
    
    # Extrair métricas para tabela final
    AVG_MS=$(echo "$OUTPUT" | grep "Avg frame time:" | awk '{print $4}')
    FPS=$(echo "$OUTPUT" | grep "Avg FPS:" | awk '{print $3}')
    MPPS=$(echo "$OUTPUT" | grep "Pixels/sec:" | sed 's/.*Pixels\/sec: *\([0-9.]*\).*/\1/')
    BW=$(echo "$OUTPUT" | grep "VRAM bandwidth:" | sed 's/.*VRAM bandwidth: *\([0-9.]*\).*/\1/')
    RES=$(echo "$OUTPUT" | grep "Resolution:" | awk -F': ' '{print $2}')
    
    echo "$bm|$RES|$AVG_MS|$FPS|$MPPS|$BW" >> "$RESULTS_FILE"
done

# ============================================================
# Tabela de resultados
# ============================================================
echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║              SUMMARY TABLE                     ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════╝${NC}"
echo ""
printf "${BOLD}%-28s %-16s %10s %10s %10s %10s${NC}\n" \
    "Benchmark" "Resolution" "Avg(ms)" "FPS" "MP/s" "BW(MB/s)"
echo "──────────────────────────────────────────────────────────────────────────────"

while IFS='|' read -r name res avg fps mpps bw; do
    # Colorir FPS
    FPS_NUM=$(echo "$fps" | cut -d. -f1)
    if [ "$FPS_NUM" -gt 30 ] 2>/dev/null; then
        FPS_COLOR="${GREEN}"
    elif [ "$FPS_NUM" -gt 10 ] 2>/dev/null; then
        FPS_COLOR="${YELLOW}"
    else
        FPS_COLOR="${RED}"
    fi
    
    printf "%-28s %-16s %10s ${FPS_COLOR}%10s${NC} %10s %10s\n" \
        "$name" "$res" "$avg" "$fps" "$mpps" "$bw"
done < "$RESULTS_FILE"

echo "──────────────────────────────────────────────────────────────────────────────"
echo ""
echo -e "${GREEN}Benchmark completo! Resultados salvos em: $RESULTS_FILE${NC}"
echo ""

rm -f "$RESULTS_FILE"
