CC = gcc
WASM3_DIR = wasm3/source

CFLAGS ?= -I$(WASM3_DIR) -I../../include -O2
LDFLAGS = -lm
WASM3_REPO = https://github.com/wasm3/wasm3.git
WASM3_SRC = $(wildcard $(WASM3_DIR)/*.c)

HOST = wagnostic

.PHONY: all host clean

all: host

$(WASM3_DIR)/wasm3.h:
	@if [ ! -d "$(WASM3_DIR)" ]; then \
		echo "Cloning wasm3 from $(WASM3_REPO)..."; \
		git clone --depth 1 $(WASM3_REPO) wasm3; \
	fi

host: $(HOST)

$(HOST): host.c gif_encoder.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(HOST)
