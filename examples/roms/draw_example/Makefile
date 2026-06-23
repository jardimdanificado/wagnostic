CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=8388608
IMG2WASM = ../../tools/img2wasm.sh

TARGET = ../../draw_example.wasm

all: $(TARGET)

image.rgb565.h: image.webp
	$(IMG2WASM) $< --rgb565

$(TARGET): main.c olive.c image.rgb565.h
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c olive.c

clean:
	rm -f $(TARGET) image.rgb565.h image.raw image.h

.PHONY: all clean
