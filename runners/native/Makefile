CC = gcc
WASM3_DIR = wasm3/source

CFLAGS ?= -I$(WASM3_DIR) -I../../include -O2
LDFLAGS = -lm
WASM3_REPO = https://github.com/wasm3/wasm3.git
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
CFLAGS ?= -I$(WASM3_DIR) -O2

HOST = wagnostic

.PHONY: all host clean

all: host

# Auto-clone wasm3 if missing
$(WASM3_DIR)/m3_api_libc.c:
	@echo "Cloning wasm3 from $(WASM3_REPO)..."
	git clone --depth 1 $(WASM3_REPO) wasm3

host: $(HOST)

$(HOST): host.c platform.c ipc.c worker.c gif_encoder.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(HOST)
