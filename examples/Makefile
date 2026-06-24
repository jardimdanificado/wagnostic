# =============================================================================
#  WAGNOSTIC — Root Makefile
# =============================================================================
# Delegates to sub-Makefiles in roms/ and runners/.
# Run `make` to build the default native host + example ROMs.
# Run `make help` for all targets.
# =============================================================================

# ── ROM directories ──────────────────────────────────────────────────────────
ROM_DIRS = \
	roms/buttons_test \
	roms/fallback_test \
	roms/terminal_test

BENCHMARK_DIRS =

# ── Runner directories ───────────────────────────────────────────────────────
RUNNER_DIRS = runners/native runners/native-spidermonkey

# ── ROM compilation flags (exported so sub-Makefiles can use them) ───────────
export CLANG = clang
export CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w
export LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
export MEM_FLAGS = -Wl,--initial-memory=8388608

# ── Host build flags ────────────────────────────────────────────────────────
CC = gcc
CXX = g++
WASM3_DIR = runners/lib/wasm3/source
WASMTIME_DIR = runners/lib/wasmtime

HOST_CFLAGS = -I$(WASM3_DIR) -O2
HOST_LDFLAGS = -lSDL2 -lGL -lm

WASMTIME_CFLAGS = -I$(WASMTIME_DIR)/include
WASMTIME_LDFLAGS = -L$(WASMTIME_DIR)/lib -lwasmtime -lpthread -ldl -lm

# SpiderMonkey flags
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-140)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-140)

# ── Binary targets (root level) ──────────────────────────────────────────────
TARGET_HOST           = wagnostic
TARGET_HOST_GPU       = wagnostic-gpu
TARGET_HOST_WASMTIME  = wagnostic-wasmtime
TARGET_HOST_BENCH     = wagnostic-bench
TARGET_HOST_BENCH_WASMTIME = wagnostic-bench-wasmtime
TARGET_HOST_BENCH_SM  = wagnostic-bench-sm
TARGET_HOST_SM = wagnostic-sm

# ── Phony targets ────────────────────────────────────────────────────────────
.PHONY: all help clean \
        host host-wasmtime host-sm \
        roms

# ═════════════════════════════════════════════════════════════════════════════
#  DEFAULT TARGET
# ═════════════════════════════════════════════════════════════════════════════
all: host roms

# ═════════════════════════════════════════════════════════════════════════════
#  HELP
# ═════════════════════════════════════════════════════════════════════════════
help:
	@echo "Wagnostic Build System"
	@echo ""
	@echo "  Targets:"
	@echo "    all              Build native host + example ROMs (default)"
	@echo "    host             Build native host (wasm3 + SDL2)"
	@echo "    host-wasmtime    Build Wasmtime JIT host"
	@echo "    host-sm          Build SpiderMonkey host (SDL2+OpenGL)"
	@echo "    roms             Build example ROMs"
	@echo "    clean            Remove all build artifacts"
	@echo ""
	@echo "  Build individual ROM:  make -C roms/<name>"
	@echo "  Build individual runner:  make -C runners/<name>"

# ═════════════════════════════════════════════════════════════════════════════
#  HOST TARGETS  (simple targets — compile directly, not via sub-Makefiles)
WASM3_SRC = $(WASM3_DIR)/m3_api_libc.c \
            $(WASM3_DIR)/m3_bind.c \
            $(WASM3_DIR)/m3_code.c \
            $(WASM3_DIR)/m3_compile.c \
            $(WASM3_DIR)/m3_core.c \
            $(WASM3_DIR)/m3_env.c \
            $(WASM3_DIR)/m3_exec.c \
            $(WASM3_DIR)/m3_function.c \
            $(WASM3_DIR)/m3_info.c \
            $(WASM3_DIR)/m3_module.c \
            $(WASM3_DIR)/m3_parse.c

host: $(TARGET_HOST)

$(TARGET_HOST): runners/native/host.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ -lSDL2 -lm

host-gpu: $(TARGET_HOST_GPU)

$(TARGET_HOST_GPU): runners/native_gpu/host_gpu.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ $(HOST_LDFLAGS)

host-wasmtime: $(TARGET_HOST_WASMTIME)

$(TARGET_HOST_WASMTIME): runners/native-wasmtime/host.c
	$(CC) $(WASMTIME_CFLAGS) $< -o $@ $(HOST_LDFLAGS) $(WASMTIME_LDFLAGS)

host-sm: $(TARGET_HOST_SM)

$(TARGET_HOST_SM): runners/native-spidermonkey/host_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2 -lSDL2 -lGL

# ═════════════════════════════════════════════════════════════════════════════
#  ROM TARGETS  (delegate to sub-Makefiles)
# ═════════════════════════════════════════════════════════════════════════════
roms:
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

benchmarks:
	@for dir in $(BENCHMARK_DIRS); do \
		$(MAKE) -C $$dir || exit 1; \
	done

# ═════════════════════════════════════════════════════════════════════════════
#  CLEAN
# ═════════════════════════════════════════════════════════════════════════════
clean:
	# Clean ROMs
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	# Clean built .wasm files at root
	rm -f *.wasm
	# Clean host binaries
	rm -f $(TARGET_HOST) $(TARGET_HOST_WASMTIME) $(TARGET_HOST_SM)
	# Clean other legacy artifacts
	rm -f wagnostic-bench wagnostic-bench-wasmtime wagnostic-bench-sm
	rm -f wagnostic-bench-wamr wagnostic-bench-native wagnostic-bench-gpu wagnostic-wasmtime
	# Clean benchmark dir
	rm -rf roms/benchmark
