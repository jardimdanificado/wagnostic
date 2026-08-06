# =============================================================================
#  WAGNOSTIC — ROMs Makefile
# =============================================================================

ROM_DIRS = \
	display_test \
	input_test \
	full_test \
	full_test_static \
	full_test_dynamic \
	buttons_test \
	fallback_test \
	bpp1_test \
	bpp2_test \
	bpp4_test \
	exotic1_test \
	exotic2_test \
	mquickjs_wagner \
	microqjs_raw


WASM3_BIN ?= $(firstword $(wildcard ../build/wagnostic ../examples/wagnostic wagnostic))
GIFNOSTIC_BIN ?= $(firstword $(wildcard ../build/gifnostic ../examples/gifnostic gifnostic))
NODE_HOST ?= ../emulators/node/wagnostic.js

RUNNER ?= wasm3

.PHONY: all clean test test-wasm3 test-node test-gifnostic $(ROM_DIRS)

all: $(ROM_DIRS)

$(ROM_DIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f *.wasm

test:
	@if [ "$(RUNNER)" = "wasm3" ]; then \
		$(MAKE) test-wasm3; \
	elif [ "$(RUNNER)" = "node" ]; then \
		$(MAKE) test-node; \
	elif [ "$(RUNNER)" = "gifnostic" ]; then \
		$(MAKE) test-gifnostic; \
	else \
		echo "Unknown RUNNER '$(RUNNER)'. Options: wasm3, node, gifnostic"; \
		exit 1; \
	fi

test-wasm3: all
	@echo "Launching all ROMs simultaneously with WASM3 host ($(WASM3_BIN))..."
	@for rom in *.wasm; do \
		echo "Starting $$rom..."; \
		$(WASM3_BIN) $$rom & \
	done

test-node: all
	@echo "Launching all ROMs simultaneously with Node.js host ($(NODE_HOST))..."
	@for rom in *.wasm; do \
		echo "Starting $$rom..."; \
		node $(NODE_HOST) $$rom & \
	done

test-gifnostic: all
	@echo "Running all ROMs simultaneously with Gifnostic headless host ($(GIFNOSTIC_BIN))..."
	@for rom in *.wasm; do \
		echo "Testing $$rom..."; \
		$(GIFNOSTIC_BIN) -n 30 -d $$rom & \
	done
