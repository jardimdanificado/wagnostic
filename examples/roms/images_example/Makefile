CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=8388608
IMG2FB = ../../tools/img2fb.sh

TARGET = ../../images_example.wasm

all: $(TARGET) image.h image8.h image16.h image32.h

image.h: image.webp
	$(IMG2FB) image.webp image 16

image8.h: image.webp
	$(IMG2FB) image.webp image8 8

image16.h: image.webp
	$(IMG2FB) image.webp image16 16

image32.h: image.webp
	$(IMG2FB) image.webp image32 32

$(TARGET): main.c image.h
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET) image.h image8.h image16.h image32.h image.raw image8.raw image16.raw image32.raw

.PHONY: all clean
