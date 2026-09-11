CC = clang
CFLAGS = --target=wasm32 -nostdlib -O2 -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export=update -Wl,--allow-undefined

all: ../comm_broadcast_test.wasm

../comm_broadcast_test.wasm: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f ../comm_broadcast_test.wasm
