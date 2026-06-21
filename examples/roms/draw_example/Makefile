CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=8388608
IMG2FB = ../../tools/img2fb.sh

TARGET = ../../draw_example.wasm

all: $(TARGET)

image.h: image.webp
	$(IMG2FB) image.webp image 16

$(TARGET): main.c olive.c image.h
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c olive.c

clean:
	rm -f $(TARGET) image.h image.raw

.PHONY: all clean
