# =============================================================================
#  PIOLHO — ROMs Makefile
# =============================================================================

ROM_DIRS = \
	bare_counter \
	use_test \
	logger_example \
	ipc_producer \
	ipc_consumer \
	comm_tcp_test \
	comm_pipe_test \
	comm_ws_test \
	comm_udp_test \
	comm_workers_test

TEST_DIRS = \
	bare_counter \
	use_test \
	logger_example \
	ipc_producer \
	comm_tcp_test \
	comm_pipe_test \
	comm_ws_test \
	comm_udp_test \
	comm_workers_test

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
	@echo "Running all standalone communication and runtime test hosts..."
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
