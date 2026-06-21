CC = gcc
WASM3_DIR = ../lib/wasm3/source
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
CFLAGS = -I$(WASM3_DIR) -O2
LDFLAGS = -lSDL2 -lm

HOST = ../../wagnostic
BENCH = ../../wagnostic-bench

.PHONY: all host bench clean

all: host bench

host: $(HOST)

$(HOST): host.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bench: $(BENCH)

$(BENCH): host_benchmark.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ -lm

clean:
	rm -f $(HOST) $(BENCH)
