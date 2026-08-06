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
	@rm -rf /tmp/wagnostic_test_logs && mkdir -p /tmp/wagnostic_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		($(WASM3_BIN) $$rom > /tmp/wagnostic_test_logs/$$rom.log 2>&1; echo $$? > /tmp/wagnostic_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	sleep 1; \
	failed=0; \
	echo ""; \
	echo "===================================================="; \
	echo " WASM3 INITIALIZATION MONITOR"; \
	echo "===================================================="; \
	for rom in *.wasm; do \
		if [ -f /tmp/wagnostic_test_logs/$$rom.code ]; then \
			code=$$(cat /tmp/wagnostic_test_logs/$$rom.code); \
			if [ "$$code" != "0" ]; then \
				printf "  [\033[31mFAILED\033[0m] %s (Crashed on startup with code %s)\n" "$$rom" "$$code"; \
				cat /tmp/wagnostic_test_logs/$$rom.log; \
				failed=$$((failed + 1)); \
			fi; \
		else \
			printf "  [\033[32m RUNNING \033[0m] %s\n" "$$rom"; \
		fi; \
	done; \
	echo "===================================================="; \
	if [ $$failed -gt 0 ]; then exit 1; fi

test-node: all
	@echo "Launching all ROMs simultaneously with Node.js host ($(NODE_HOST))..."
	@rm -rf /tmp/wagnostic_test_logs && mkdir -p /tmp/wagnostic_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		(node $(NODE_HOST) $$rom > /tmp/wagnostic_test_logs/$$rom.log 2>&1; echo $$? > /tmp/wagnostic_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	sleep 1; \
	failed=0; \
	echo ""; \
	echo "===================================================="; \
	echo " NODE.JS INITIALIZATION MONITOR"; \
	echo "===================================================="; \
	for rom in *.wasm; do \
		if [ -f /tmp/wagnostic_test_logs/$$rom.code ]; then \
			code=$$(cat /tmp/wagnostic_test_logs/$$rom.code); \
			if [ "$$code" != "0" ]; then \
				printf "  [\033[31mFAILED\033[0m] %s (Crashed on startup with code %s)\n" "$$rom" "$$code"; \
				cat /tmp/wagnostic_test_logs/$$rom.log; \
				failed=$$((failed + 1)); \
			fi; \
		else \
			printf "  [\033[32m RUNNING \033[0m] %s\n" "$$rom"; \
		fi; \
	done; \
	echo "===================================================="; \
	if [ $$failed -gt 0 ]; then exit 1; fi

test-gifnostic: all
	@echo "Running all ROMs simultaneously with Gifnostic headless host ($(GIFNOSTIC_BIN))..."
	@rm -rf /tmp/wagnostic_test_logs && mkdir -p /tmp/wagnostic_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		($(GIFNOSTIC_BIN) -n 30 $$rom > /tmp/wagnostic_test_logs/$$rom.log 2>&1; echo $$? > /tmp/wagnostic_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	echo ""; \
	echo "===================================================="; \
	echo " GIFNOSTIC MASS TEST RESULTS"; \
	echo "===================================================="; \
	failed=0; total=0; \
	for rom in *.wasm; do \
		total=$$((total + 1)); \
		code=$$(cat /tmp/wagnostic_test_logs/$$rom.code 2>/dev/null || echo 1); \
		if [ "$$code" = "0" ]; then \
			printf "  [\033[32m  OK  \033[0m] %s\n" "$$rom"; \
		else \
			printf "  [\033[31mFAILED\033[0m] %s (Exit Code: %s)\n" "$$rom" "$$code"; \
			cat /tmp/wagnostic_test_logs/$$rom.log; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "===================================================="; \
	echo " Total: $$total | Passed: $$((total - failed)) | Failed: $$failed"; \
	echo "===================================================="; \
	rm -rf /tmp/wagnostic_test_logs; \
	if [ $$failed -gt 0 ]; then exit 1; fi
