WASM3_DIR := ../wasm3/wasm3/source
WASM3_SRCS := $(wildcard $(WASM3_DIR)/*.c)

.PHONY: all clean

all: libwasm3.a wagnostic-v

libwasm3.a: $(WASM3_SRCS) miniz.c
	cc -c -O2 -I$(WASM3_DIR) $(WASM3_SRCS) miniz.c
	ar rcs $@ *.o
	rm -f *.o

wagnostic-v: host.v libwasm3.a
	v -cg -gc none -enable-globals -cc gcc host.v -o $@

clean:
	rm -f wagnostic-v libwasm3.a
