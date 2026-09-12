# =============================================================================
#  WAGNOSTIC 2.0 — ROMs Makefile
# =============================================================================

ROM_DIRS = \
	display_test \
	input_test \
	buttons_test \
	full_test \
	wextension_test \
	bare_counter \
	logger_example

WAGNOSTIC_BIN ?= $(firstword $(wildcard ../runners/native/wagnostic ../build/wagnostic ../wagnostic wagnostic))
NODE_HOST ?= ../runners/node/wagnostic.js

RUNNER ?= native

.PHONY: all clean test test-native test-node $(ROM_DIRS)

all: $(ROM_DIRS)

$(ROM_DIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f *.wasm

test:
	@if [ "$(RUNNER)" = "native" ] || [ "$(RUNNER)" = "wasm3" ]; then \
		$(MAKE) test-native; \
	elif [ "$(RUNNER)" = "node" ]; then \
		$(MAKE) test-node; \
	else \
		echo "Unknown RUNNER '$(RUNNER)'. Options: native, node"; \
		exit 1; \
	fi

test-native: all
	@echo "Running all ROMs with Native Host ($(WAGNOSTIC_BIN) -n 30 --headless)..."
	@rm -rf /tmp/wagnostic_test_logs && mkdir -p /tmp/wagnostic_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		($(WAGNOSTIC_BIN) -n 30 --headless $$rom > /tmp/wagnostic_test_logs/$$rom.log 2>&1; echo $$? > /tmp/wagnostic_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	echo ""; \
	echo "===================================================="; \
	echo " NATIVE RUNNER TEST RESULTS"; \
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

test-node: all
	@echo "Running all ROMs with Node.js Host (node $(NODE_HOST) -n 30 --headless)..."
	@rm -rf /tmp/wagnostic_test_logs && mkdir -p /tmp/wagnostic_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		(node $(NODE_HOST) -n 30 --headless $$rom > /tmp/wagnostic_test_logs/$$rom.log 2>&1; echo $$? > /tmp/wagnostic_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	echo ""; \
	echo "===================================================="; \
	echo " NODE.JS RUNNER TEST RESULTS"; \
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
