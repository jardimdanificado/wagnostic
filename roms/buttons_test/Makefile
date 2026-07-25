CLANG ?= clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined

TARGET = ../$(shell basename $(CURDIR)).wasm

all: $(TARGET)

$(TARGET): main.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
