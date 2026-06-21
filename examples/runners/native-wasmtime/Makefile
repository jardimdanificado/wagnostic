CC = gcc
WASMTIME_DIR = ../lib/wasmtime
CFLAGS = -I$(WASMTIME_DIR)/include -O2
LDFLAGS = -L$(WASMTIME_DIR)/lib -lwasmtime -lpthread -ldl -lm -lSDL2 -lGL

HOST = ../../wagnostic-wasmtime
BENCH = ../../wagnostic-bench-wasmtime

.PHONY: all host bench clean

all: host bench

host: $(HOST)

$(HOST): host.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

bench: $(BENCH)

$(BENCH): host_benchmark.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(HOST) $(BENCH)
