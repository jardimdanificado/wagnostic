# =============================================================================
#  mquickjs_wagner ROM — Wagner API + MicroQuickJS embedded scripting
# =============================================================================
#
#  This ROM bundles MicroQuickJS inside a Wagnostic WASM ROM and exposes
#  Wagner C drawing & state-machine functions directly to JavaScript.
#
#  Usage:
#    make            → builds ../mquickjs_wagner.wasm
#    make run        → builds and runs with wagnostic
#    make clean      → removes build artefacts
#
# =============================================================================

CLANG    ?= clang
WAGNOSTIC ?= ../../wagnostic

WAGNER_ROOT := ../../wagner
MQJS        := mquickjs
SHIM        := shim

TARGET = ../mquickjs_wagner.wasm

CFLAGS = \
	--target=wasm32 \
	-nostdlib \
	-fno-delete-null-pointer-checks \
	-O3 \
	-w \
	-isystem include \
	-I$(MQJS) \
	-I$(WAGNER_ROOT) \
	-I$(WAGNER_ROOT)/lib/decoders \
	-DWAGNER_CFG_BPP=32 \
	-DWAGNER_NO_PNG_DECODE \
	-DWAGNER_NO_AUDIO_DECODE \
	-I.

LDFLAGS = \
	-Wl,--no-entry \
	-Wl,--export-all \
	-Wl,--allow-undefined \
	-Wl,--initial-memory=2097152

# All C sources that make up the ROM
SRCS = \
	main.c \
	$(MQJS)/mquickjs.c \
	$(MQJS)/cutils.c \
	$(MQJS)/dtoa.c \
	$(MQJS)/libm.c \
	shim/libc_shim.c

.PHONY: all run clean gen-assets patch-stdlib

all: patch-stdlib gen-assets $(TARGET)

$(MQJS):
	@echo "Fetching MicroQuickJS repository..."
	git clone --depth 1 https://github.com/bellard/mquickjs.git $(MQJS)

patch-stdlib: $(MQJS)
	@echo "Generating MicroQuickJS headers for Wagner..."
	@python3 patch_stdlib.py
	@make -C $(MQJS) mqjs_stdlib.h mquickjs_atom.h HOST_CC="gcc -m32" HOST_CFLAGS="-m32 -O2" HOST_LDFLAGS="-m32"

$(TARGET): $(SRCS) main.c assets.h
	$(CLANG) $(CFLAGS) $(LDFLAGS) -o $@ $(SRCS)
	@echo "✓  Built $(TARGET)  ($$(wc -c < $(TARGET)) bytes)"

# Regenerate assets.h from every file inside assets/
gen-assets:
	@python3 gen_assets.py

run: all
	$(WAGNOSTIC) $(TARGET)

clean:
	rm -f $(TARGET)
