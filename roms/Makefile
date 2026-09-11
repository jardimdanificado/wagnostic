# =============================================================================
#  PIOLHO — ROMs Makefile
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
	wextension_test \
	bare_counter \
	logger_example \
	ipc_producer \
	ipc_consumer \
	pathtracer_master \
	pathtracer_worker


PIOLHO_BIN ?= $(firstword $(wildcard ../build/piolho ../piolho piolho))
NODE_HOST ?= ../bin/piolho.js

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

test: all
	@echo "Running all ROMs with Universal Host (node $(NODE_HOST) -n 30)..."
	@rm -rf /tmp/piolho_test_logs && mkdir -p /tmp/piolho_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		if [ "$$rom" = "ipc_producer.wasm" ] || [ "$$rom" = "ipc_consumer.wasm" ] || [ "$$rom" = "pathtracer_worker.wasm" ]; then continue; fi; \
		(node $(NODE_HOST) -n 10 $$rom > /tmp/piolho_test_logs/$$rom.log 2>&1; echo $$? > /tmp/piolho_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	(node $(NODE_HOST) ipc_producer.wasm:producer ipc_consumer.wasm:consumer > /tmp/piolho_test_logs/multi_rom_ipc.log 2>&1; echo $$? > /tmp/piolho_test_logs/multi_rom_ipc.code); \
	(node $(NODE_HOST) -n 5 pathtracer_master.wasm:master pathtracer_worker.wasm:worker0 pathtracer_worker.wasm:worker1 > /tmp/piolho_test_logs/pathtracer_parallel.log 2>&1; echo $$? > /tmp/piolho_test_logs/pathtracer_parallel.code); \
	echo ""; \
	echo "===================================================="; \
	echo " UNIVERSAL HOST MASS TEST RESULTS"; \
	echo "===================================================="; \
	failed=0; total=0; \
	for rom in *.wasm; do \
		if [ "$$rom" = "ipc_producer.wasm" ] || [ "$$rom" = "ipc_consumer.wasm" ] || [ "$$rom" = "pathtracer_worker.wasm" ]; then continue; fi; \
		total=$$((total + 1)); \
		code=$$(cat /tmp/piolho_test_logs/$$rom.code 2>/dev/null || echo 1); \
		if [ "$$code" = "0" ]; then \
			printf "  [\033[32m  OK  \033[0m] %s\n" "$$rom"; \
		else \
			printf "  [\033[31mFAILED\033[0m] %s (Exit Code: %s)\n" "$$rom" "$$code"; \
			cat /tmp/piolho_test_logs/$$rom.log; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	total=$$((total + 1)); \
	code=$$(cat /tmp/piolho_test_logs/multi_rom_ipc.code 2>/dev/null || echo 1); \
	if [ "$$code" = "0" ]; then \
		printf "  [\033[32m  OK  \033[0m] Multi-ROM Rendezvous IPC (producer <-> consumer)\n"; \
	else \
		printf "  [\033[31mFAILED\033[0m] Multi-ROM Rendezvous IPC (Exit Code: %s)\n" "$$code"; \
		cat /tmp/piolho_test_logs/multi_rom_ipc.log; \
		failed=$$((failed + 1)); \
	fi; \
	total=$$((total + 1)); \
	code=$$(cat /tmp/piolho_test_logs/pathtracer_parallel.code 2>/dev/null || echo 1); \
	if [ "$$code" = "0" ]; then \
		printf "  [\033[32m  OK  \033[0m] Parallel Path Tracer Multi-Worker (master + 2 workers)\n"; \
	else \
		printf "  [\033[31mFAILED\033[0m] Parallel Path Tracer Multi-Worker (Exit Code: %s)\n" "$$code"; \
		cat /tmp/piolho_test_logs/pathtracer_parallel.log; \
		failed=$$((failed + 1)); \
	fi; \
	echo "===================================================="; \
	echo " Total: $$total | Passed: $$((total - failed)) | Failed: $$failed"; \
	echo "===================================================="; \
	rm -rf /tmp/piolho_test_logs; \
	if [ $$failed -gt 0 ]; then exit 1; fi

test-node: test
	@echo "Running all ROMs with Node.js Host (node $(NODE_HOST) -n 30 --headless)..."
	@rm -rf /tmp/piolho_test_logs && mkdir -p /tmp/piolho_test_logs
	@pids=""; \
	for rom in *.wasm; do \
		if [ "$$rom" = "ipc_producer.wasm" ] || [ "$$rom" = "ipc_consumer.wasm" ] || [ "$$rom" = "pathtracer_worker.wasm" ]; then continue; fi; \
		(node $(NODE_HOST) -n 10 --headless $$rom > /tmp/piolho_test_logs/$$rom.log 2>&1; echo $$? > /tmp/piolho_test_logs/$$rom.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	(node $(NODE_HOST) --headless ipc_producer.wasm:producer ipc_consumer.wasm:consumer > /tmp/piolho_test_logs/multi_rom_ipc.log 2>&1; echo $$? > /tmp/piolho_test_logs/multi_rom_ipc.code); \
	(node $(NODE_HOST) -n 5 --headless pathtracer_master.wasm:master pathtracer_worker.wasm:worker0 pathtracer_worker.wasm:worker1 > /tmp/piolho_test_logs/pathtracer_parallel.log 2>&1; echo $$? > /tmp/piolho_test_logs/pathtracer_parallel.code); \
	echo ""; \
	echo "===================================================="; \
	echo " NODE.JS RUNNER MASS TEST RESULTS"; \
	echo "===================================================="; \
	failed=0; total=0; \
	for rom in *.wasm; do \
		if [ "$$rom" = "ipc_producer.wasm" ] || [ "$$rom" = "ipc_consumer.wasm" ] || [ "$$rom" = "pathtracer_worker.wasm" ]; then continue; fi; \
		total=$$((total + 1)); \
		code=$$(cat /tmp/piolho_test_logs/$$rom.code 2>/dev/null || echo 1); \
		if [ "$$code" = "0" ]; then \
			printf "  [\033[32m  OK  \033[0m] %s\n" "$$rom"; \
		else \
			printf "  [\033[31mFAILED\033[0m] %s (Exit Code: %s)\n" "$$rom" "$$code"; \
			cat /tmp/piolho_test_logs/$$rom.log; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	total=$$((total + 1)); \
	code=$$(cat /tmp/piolho_test_logs/multi_rom_ipc.code 2>/dev/null || echo 1); \
	if [ "$$code" = "0" ]; then \
		printf "  [\033[32m  OK  \033[0m] Multi-ROM Rendezvous IPC (producer <-> consumer)\n"; \
	else \
		printf "  [\033[31mFAILED\033[0m] Multi-ROM Rendezvous IPC (Exit Code: %s)\n" "$$code"; \
		cat /tmp/piolho_test_logs/multi_rom_ipc.log; \
		failed=$$((failed + 1)); \
	fi; \
	total=$$((total + 1)); \
	code=$$(cat /tmp/piolho_test_logs/pathtracer_parallel.code 2>/dev/null || echo 1); \
	if [ "$$code" = "0" ]; then \
		printf "  [\033[32m  OK  \033[0m] Parallel Path Tracer Multi-Worker (master + 2 workers)\n"; \
	else \
		printf "  [\033[31mFAILED\033[0m] Parallel Path Tracer Multi-Worker (Exit Code: %s)\n" "$$code"; \
		cat /tmp/piolho_test_logs/pathtracer_parallel.log; \
		failed=$$((failed + 1)); \
	fi; \
	echo "===================================================="; \
	echo " Total: $$total | Passed: $$((total - failed)) | Failed: $$failed"; \
	echo "===================================================="; \
	rm -rf /tmp/piolho_test_logs; \
	if [ $$failed -gt 0 ]; then exit 1; fi

