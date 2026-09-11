CLANG ?= clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--initial-memory=131072

TARGET = ../comm_udp_test.wasm

all: $(TARGET)

$(TARGET): main.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
