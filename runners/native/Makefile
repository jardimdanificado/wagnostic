CC = gcc
WASM3_DIR = wasm3/source

CFLAGS ?= -I$(WASM3_DIR) -I../../include -O2
LDFLAGS = -lm
WASM3_REPO = https://github.com/wasm3/wasm3.git

HOST = wagnostic

.PHONY: all host clean wasm3_clone

all: host

wasm3_clone:
	@if [ ! -d "$(WASM3_DIR)" ]; then \
		echo "Cloning wasm3 from $(WASM3_REPO)..."; \
		git clone --depth 1 $(WASM3_REPO) wasm3; \
	fi

host: wasm3_clone
	$(MAKE) $(HOST)

$(HOST): host.c gif_encoder.c
	$(CC) $(CFLAGS) host.c gif_encoder.c $(wildcard $(WASM3_DIR)/*.c) -o $@ $(LDFLAGS)

clean:
	rm -f $(HOST)
