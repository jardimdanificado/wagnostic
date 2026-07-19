# =============================================================================
#  WAGNOSTIC — Root Makefile
# =============================================================================
# Delegates to sub-Makefiles in roms/ and ../runners/.
# Run `make` to build the default native host + example ROMs.
# Run `make help` for all targets.
# =============================================================================

# ── ROM directories ──────────────────────────────────────────────────────────
ROM_DIRS = \
	roms/display_test \
	roms/input_test \
	roms/audio_wav \
	roms/audio_mp3 \
	roms/audio_ogg \
	roms/full_test \
	roms/full_test_static \
	roms/full_test_dynamic \
	roms/buttons_test \
	roms/fallback_test \
	roms/bpp1_test \
	roms/bpp2_test \
	roms/bpp4_test \
	roms/exotic1_test \
	roms/exotic2_test



# ── Runner directories ───────────────────────────────────────────────────────
RUNNER_DIRS = ../runners/wasm3 ../runners/spidermonkey

# ── ROM compilation flags (exported so sub-Makefiles can use them) ───────────
export CLANG = clang
export CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w
export LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
export MEM_FLAGS = 

# ── Host build flags ────────────────────────────────────────────────────────
CC = gcc
CXX = g++
WASM3_DIR = ../runners/wasm3/wasm3/source
WASM3_REPO = https://github.com/wasm3/wasm3.git

HOST_CFLAGS = -I$(WASM3_DIR) -O2
HOST_LDFLAGS = -lSDL2 -lGL -lm



# SpiderMonkey flags — try mozjs-128 (Ubuntu 24.04), fall back to mozjs-140
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-128 2>/dev/null || pkg-config --cflags mozjs-140 2>/dev/null)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-128 2>/dev/null || pkg-config --libs mozjs-140 2>/dev/null)

# ── Binary targets (root level) ──────────────────────────────────────────────
TARGET_HOST           = wagnostic

TARGET_HOST_SM = wagnostic-sm

# ── Phony targets ────────────────────────────────────────────────────────────
.PHONY: all help clean \
	host host-sm \
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

	@echo "    host-sm          Build SpiderMonkey host (SDL2+OpenGL)"
	@echo "    roms             Build example ROMs"
	@echo "    clean            Remove all build artifacts"
	@echo ""
	@echo "  Build individual ROM:  make -C roms/<name>"
	@echo "  Build individual runner:  make -C ../runners/<name> (wasm3|spidermonkey)"

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

# Auto-clone wasm3 if missing
$(WASM3_DIR)/m3_api_libc.c:
	@echo "Cloning wasm3 from $(WASM3_REPO)..."
	git clone --depth 1 $(WASM3_REPO) ../runners/wasm3/wasm3

host: $(TARGET_HOST)

$(TARGET_HOST): ../runners/wasm3/host.c $(WASM3_DIR)/m3_api_libc.c
	$(CC) $(HOST_CFLAGS) ../runners/wasm3/host.c $(WASM3_SRC) -o $@ -lSDL2 -lm

host-sm: $(TARGET_HOST_SM)

$(TARGET_HOST_SM): ../runners/spidermonkey/host_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) ../runners/spidermonkey/host_sm.cpp -o $@ $(SM_LDFLAGS) -O2 -lSDL2 -lGL

# ═════════════════════════════════════════════════════════════════════════════
#  ROM TARGETS  (delegate to sub-Makefiles)
# ═════════════════════════════════════════════════════════════════════════════
roms:
	@for dir in $(ROM_DIRS); do \
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
	rm -f $(TARGET_HOST) $(TARGET_HOST_SM)
