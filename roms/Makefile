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
	use_test \
	bare_counter \
	logger_example \
	ipc_producer \
	ipc_consumer \
	pathtracer_master \
	pathtracer_worker \
	comm_tcp_test \
	comm_pipe_test \
	comm_ws_test \
	comm_udp_test


TEST_DIRS = \
	bare_counter \
	display_test \
	input_test \
	buttons_test \
	fallback_test \
	bpp1_test \
	bpp2_test \
	bpp4_test \
	exotic1_test \
	exotic2_test \
	use_test \
	full_test \
	full_test_static \
	full_test_dynamic \
	logger_example \
	ipc_producer \
	pathtracer_master \
	comm_tcp_test \
	comm_pipe_test \
	comm_ws_test \
	comm_udp_test

.PHONY: all clean test $(ROM_DIRS)

all: $(ROM_DIRS)

$(ROM_DIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(ROM_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	rm -f *.wasm

test: all
	@echo "Running all standalone example hosts (node <dir>/host.js)..."
	@rm -rf /tmp/piolho_test_logs && mkdir -p /tmp/piolho_test_logs
	@pids=""; \
	for dir in $(TEST_DIRS); do \
		(node $$dir/host.js > /tmp/piolho_test_logs/$$dir.log 2>&1; echo $$? > /tmp/piolho_test_logs/$$dir.code) & \
		pids="$$pids $$!"; \
	done; \
	for pid in $$pids; do wait $$pid 2>/dev/null; done; \
	echo ""; \
	echo "===================================================="; \
	echo " STANDALONE EXAMPLE HOSTS TEST RESULTS"; \
	echo "===================================================="; \
	failed=0; total=0; \
	for dir in $(TEST_DIRS); do \
		total=$$((total + 1)); \
		code=$$(cat /tmp/piolho_test_logs/$$dir.code 2>/dev/null || echo 1); \
		if [ "$$code" = "0" ]; then \
			printf "  [\033[32m  OK  \033[0m] %s/host.js\n" "$$dir"; \
		else \
			printf "  [\033[31mFAILED\033[0m] %s/host.js (Exit Code: %s)\n" "$$dir" "$$code"; \
			cat /tmp/piolho_test_logs/$$dir.log; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "===================================================="; \
	echo " Total: $$total | Passed: $$((total - failed)) | Failed: $$failed"; \
	echo "===================================================="; \
	rm -rf /tmp/piolho_test_logs; \
	if [ $$failed -gt 0 ]; then exit 1; fi

