CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=8388608
IMG2WASM = ../../tools/img2wasm.sh

TARGET = ../../images_example.wasm

# Image headers in format-specific extensions
IMAGE_565 = image.rgb565.h
IMAGE_332 = image.rgb332.h
IMAGE_RGBA = image.rgba.h

all: $(TARGET) $(IMAGE_565) $(IMAGE_332) $(IMAGE_RGBA)

$(IMAGE_565): image.webp
	$(IMG2WASM) $< --rgb565

$(IMAGE_332): image.webp
	$(IMG2WASM) $< --rgb332

$(IMAGE_RGBA): image.webp
	$(IMG2WASM) $< --rgba

$(TARGET): main.c $(IMAGE_565)
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET) $(IMAGE_565) $(IMAGE_332) $(IMAGE_RGBA) image.raw image8.raw image16.raw image32.raw image.h image8.h image16.h image32.h

.PHONY: all clean
