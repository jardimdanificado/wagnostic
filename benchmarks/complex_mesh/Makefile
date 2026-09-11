CC = clang
CFLAGS = --target=wasm32 -nostdlib -O3 -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export=update -Wl,--allow-undefined

all: hub_node.wasm client_tcp_pipe.wasm client_ws_broadcast.wasm echo_sibling.wasm

hub_node.wasm: hub_node.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ hub_node.c

client_tcp_pipe.wasm: client_tcp_pipe.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ client_tcp_pipe.c

client_ws_broadcast.wasm: client_ws_broadcast.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ client_ws_broadcast.c

echo_sibling.wasm: echo_sibling.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ echo_sibling.c

clean:
	rm -f *.wasm
