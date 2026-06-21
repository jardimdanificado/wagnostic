#!/usr/bin/env bash
# Wagnostic Benchmark Runner
# Compila e executa todos os benchmarks em ambos os runners (CPU e GPU)
#
# Usage: ./benchmark.sh [num_frames]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

FRAMES="${1:-100}"
BENCH_HOST="./wagnostic-bench"
BENCH_WASMTIME="./wagnostic-bench-wasmtime"
BENCH_NODE="./wagnostic-bench-node"
BENCH_SM="./wagnostic-bench-sm"
BENCH_JS140="./wagnostic-bench-js140"
BENCH_JSC="./wagnostic-bench-jsc"
BENCH_LOVE_DIR="/tmp/love-bench"
BENCH_LOVE="love"

# Forçar locale US para printf() com ponto decimal
export LC_NUMERIC=C

# Configurar LD_LIBRARY_PATH para wasmtime
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:runners/lib/wasmtime/lib"

# Cores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
NC='\033[0m'

echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║           WAGNOSTIC BENCHMARK SUITE                ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════╝${NC}"
echo ""

# ============================================================
# Step 1: Compilar hosts de benchmark
# ============================================================
echo -e "${CYAN}[1/3] Compilando hosts de benchmark...${NC}"

make host-bench 2>&1 | tail -1
if [ ! -f "$BENCH_HOST" ]; then echo -e "${RED}  ERRO: Falha ao compilar $BENCH_HOST${NC}"; exit 1; fi
echo -e "${GREEN}  OK: $BENCH_HOST${NC}"

make host-bench-wasmtime 2>&1 | tail -1
if [ ! -f "$BENCH_WASMTIME" ]; then
    echo -e "${YELLOW}  AVISO: wasmtime não disponível${NC}"; BENCH_WASMTIME=""
else
    echo -e "${GREEN}  OK: $BENCH_WASMTIME${NC}"
fi

make host-bench-node 2>&1 | tail -1
if [ ! -f "$BENCH_NODE" ]; then
    echo -e "${YELLOW}  AVISO: Node.js runner não disponível${NC}"; BENCH_NODE=""
else
    echo -e "${GREEN}  OK: $BENCH_NODE${NC}"
fi

make host-bench-sm 2>&1 | tail -1
if [ ! -f "$BENCH_SM" ]; then
    echo -e "${YELLOW}  AVISO: SpiderMonkey C++ não disponível${NC}"; BENCH_SM=""
else
    echo -e "${GREEN}  OK: $BENCH_SM${NC}"
fi

make host-bench-js140 2>&1 | tail -1
if [ ! -f "$BENCH_JS140" ]; then
    echo -e "${YELLOW}  AVISO: js140 runner não disponível${NC}"; BENCH_JS140=""
else
    echo -e "${GREEN}  OK: $BENCH_JS140${NC}"
fi

make host-bench-jsc 2>&1 | tail -1
if [ ! -f "$BENCH_JSC" ]; then
    echo -e "${YELLOW}  AVISO: JavaScriptCore runner não disponível${NC}"; BENCH_JSC=""
else
    echo -e "${GREEN}  OK: $BENCH_JSC${NC}"
fi

# Setup LÖVE benchmark project
if command -v love &>/dev/null; then
    if [ -f "runners/love/libwasm3.so" ] && [ -f "runners/love/benchmark_main.lua" ]; then
        mkdir -p "$BENCH_LOVE_DIR"
        cp runners/love/benchmark_main.lua "$BENCH_LOVE_DIR/main.lua"
        cp runners/love/libwasm3.so "$BENCH_LOVE_DIR/"
        echo -e "${GREEN}  OK: LÖVE benchmark ($(love --version 2>&1 | head -1))${NC}"
    else
        echo -e "${YELLOW}  AVISO: love-bench incompleto${NC}"; BENCH_LOVE=""
    fi
else
    echo -e "${YELLOW}  AVISO: LÖVE não instalado${NC}"; BENCH_LOVE=""
fi
echo ""

# ============================================================
# Step 2: Compilar ROMs de benchmark
# ============================================================
echo -e "${CYAN}[2/3] Compilando ROMs de benchmark...${NC}"
make benchmarks 2>&1 | grep -v "^make\[" | grep -v "^$" | head -5

BENCHMARKS=(
    "benchmark_cpu.wasm"
    "benchmark_vram.wasm"
    "benchmark_audio.wasm"
    "benchmark_particles.wasm"
    "benchmark_all.wasm"
)

for bm in "${BENCHMARKS[@]}"; do
    if [ ! -f "$bm" ]; then
        echo -e "${RED}  ERRO: Falha ao compilar $bm${NC}"
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

run_bench() {
    local runner="$1"
    local bm="$2"
    local label="$3"

    if [ -z "$runner" ] || [ ! -f "$runner" ]; then
        return 1
    fi

    echo -e "${YELLOW}  [$label] $bm${NC}"
    local OUTPUT=$("$runner" "$bm" "$FRAMES" 2>&1 | tr -d '\r')
    local EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        echo -e "${RED}    FAILED (exit $EXIT_CODE)${NC}"
        echo ""
        return 1
    fi

    # Extrair métricas
    local AVG=$(echo "$OUTPUT" | grep "Avg frame time:" | awk '{print $4}')
    local FPS=$(echo "$OUTPUT" | grep "Avg FPS:" | awk '{print $3}')
    local MPPS=$(echo "$OUTPUT" | grep "Pixels/sec:" | sed 's/.*Pixels\/sec: *\([0-9.]*\).*/\1/')
    local BW=$(echo "$OUTPUT" | grep "VRAM bandwidth:" | sed 's/.*VRAM bandwidth: *\([0-9.]*\).*/\1/')
    local RES=$(echo "$OUTPUT" | grep "Resolution:" | awk -F': ' '{print $2}' | tr -d '|')

    # Só mostrar se tiver resultado válido
    if [ -n "$AVG" ] && [ -n "$FPS" ]; then
        echo "    ${GREEN}✓${NC} ${AVG}ms/frame  |  ${FPS} FPS  |  $MPPS MP/s  |  $BW MB/s"
        echo "$bm|$label|$RES|$AVG|$FPS|$MPPS|$BW" >> "$RESULTS_FILE"
    else
        echo "    ${RED}✗ FALHOU${NC}"
    fi
    echo ""
    return 0
    return 0
}

for bm in "${BENCHMARKS[@]}"; do
    echo -e "${YELLOW}────────────────────────────────────────────────────────────${NC}"
    echo -e "${BOLD}  $bm${NC}"
    echo -e "${YELLOW}────────────────────────────────────────────────────────────${NC}"

    run_bench "$BENCH_HOST" "$bm" "native-wasm3"
    run_bench "$BENCH_WASMTIME" "$bm" "wasmtime"
    run_bench "$BENCH_NODE" "$bm" "Node.js"
    run_bench "$BENCH_SM" "$bm" "native-spidermonkey"
    run_bench "$BENCH_JS140" "$bm" "js140"
    run_bench "$BENCH_JSC" "$bm" "JavaScriptCore"
    if [ -n "$BENCH_LOVE" ]; then
        LOVE_OUTPUT=$(LD_LIBRARY_PATH="$BENCH_LOVE_DIR" "$BENCH_LOVE" "$BENCH_LOVE_DIR" "$bm" "$FRAMES" 2>&1 | tr -d '\r' | tr -d '\000')
        LOVE_AVG=$(echo "$LOVE_OUTPUT" | grep -a "Avg frame time:" | awk '{print $4}')
        LOVE_FPS=$(echo "$LOVE_OUTPUT" | grep -a "Avg FPS:" | awk '{print $3}')
        LOVE_MPPS=$(echo "$LOVE_OUTPUT" | grep -a "Pixels/sec:" | sed 's/.*Pixels\/sec: *\([0-9.]*\).*/\1/')
        LOVE_BW=$(echo "$LOVE_OUTPUT" | grep -a "VRAM bandwidth:" | sed 's/.*VRAM bandwidth: *\([0-9.]*\).*/\1/')
        LOVE_RES=$(echo "$LOVE_OUTPUT" | grep -a "Resolution:" | awk -F': ' '{print $2}' | tr -d '|')
        if [ -n "$LOVE_AVG" ]; then
            echo "    ${GREEN}✓${NC} ${LOVE_AVG}ms/frame  |  ${LOVE_FPS} FPS  |  $LOVE_MPPS MP/s  |  $LOVE_BW MB/s"
            echo "$bm|LÖVE|$LOVE_RES|$LOVE_AVG|$LOVE_FPS|$LOVE_MPPS|$LOVE_BW" >> "$RESULTS_FILE"
        else
            echo "    ${RED}✗ FALHOU${NC}"
        fi
        echo ""
    fi
done

# ============================================================
# ============================================================
# Tabela comparativa
# ============================================================
echo ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║                    COMPARATIVE TABLE                          ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

printf "${BOLD}%-26s %-16s %8s %8s %8s %8s %8s${NC}\n" \
    "Benchmark" "Runner" "Avg(ms)" "FPS" "MP/s" "BW(MB/s)" "Speedup"
echo "───────────────────────────────────────────────────────────────────────────────────"

# Ler resultados para arrays
declare -a BM_NAMES
declare -a BM_RUNNERS
declare -a BM_AVGS
declare -a BM_FPSS
declare -a BM_MPPSS
declare -a BM_BWS

while IFS='|' read -r bm_label runner_label res avg fps mpps bw; do
    [ -z "$bm_label" ] && continue
    BM_NAMES+=("$bm_label")
    BM_RUNNERS+=("$runner_label")
    BM_AVGS+=("$avg")
    BM_FPSS+=("$fps")
    BM_MPPSS+=("$mpps")
    BM_BWS+=("$bw")
done < "$RESULTS_FILE"

# Imprimir tabela
for idx in "${!BM_NAMES[@]}"; do
    bm="${BM_NAMES[$idx]}"
    runner="${BM_RUNNERS[$idx]}"
    avg="${BM_AVGS[$idx]}"
    fps="${BM_FPSS[$idx]}"
    mpps="${BM_MPPSS[$idx]}"
    bw="${BM_BWS[$idx]}"

    SPEEDUP_STR=""
    if [ "$runner" = "wasmtime" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${BOLD}${GREEN}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    elif [ "$runner" = "Node.js" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${BOLD}${CYAN}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    elif [ "$runner" = "native-spidermonkey" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${BOLD}${BLUE}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    elif [ "$runner" = "js140" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${BOLD}${CYAN}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    elif [ "$runner" = "JavaScriptCore" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${BOLD}${MAGENTA}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    elif [ "$runner" = "LÖVE" ]; then
        for jdx in "${!BM_NAMES[@]}"; do
            if [ "${BM_NAMES[$jdx]}" = "$bm" ] && [ "${BM_RUNNERS[$jdx]}" = "native-wasm3" ]; then
                cpu_avg="${BM_AVGS[$jdx]}"
                if [ -n "$cpu_avg" ] && [ -n "$avg" ]; then
                    SP=$(LC_NUMERIC=C awk "BEGIN { printf \"%.2f\", $cpu_avg / $avg }" 2>/dev/null || echo "")
                    if [ -n "$SP" ]; then
                        SP_GT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP > 1) }" 2>/dev/null || echo "0")
                        SP_LT1=$(LC_NUMERIC=C awk "BEGIN { print ($SP < 1) }" 2>/dev/null || echo "0")
                        [ "$SP_GT1" = "1" ] && SPEEDUP_STR="${YELLOW}${SP}x${NC}"
                        [ "$SP_LT1" = "1" ] && SPEEDUP_STR="${RED}${SP}x${NC}"
                    fi
                fi
                break
            fi
        done
    fi

    printf "%-26s %-16s %8.3f %8.1f %8.2f %8.2f  ${SPEEDUP_STR}\n" \
        "$bm" "$runner" "$avg" "$fps" "$mpps" "$bw"
done

echo "───────────────────────────────────────────────────────────────────────────────────"
echo -e "${GREEN}Benchmark completo! ($FRAMES frames cada)${NC}"
echo ""

rm -f "$RESULTS_FILE"
