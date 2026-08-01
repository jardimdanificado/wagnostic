CC ?= gcc
CFLAGS ?= -O3 -Wall -Wextra -I. -I$(WASM3_DIR)
WASM3_DIR = ../wasm3/wasm3/source

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

OBJS = gifnostic.o gif_encoder.o $(WASM3_SRC:.c=.o)

HOST = gifnostic

.PHONY: all clean

all: host libgifnostic.a

host: $(HOST)

$(HOST): main.c gifnostic.c gif_encoder.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ -lm

libgifnostic.a: $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(HOST) libgifnostic.a *.o $(WASM3_DIR)/*.o
