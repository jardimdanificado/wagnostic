CLANG ?= clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O0 -w -I../audio_common/shim/include -DSTB_VORBIS_NO_STDIO
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=8388608

TARGET = ../../$(shell basename $(CURDIR)).wasm

all: $(TARGET)

$(TARGET): main.c ../audio_common/shim/libc_shim.c ../audio_common/decoders/stb_vorbis.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c ../audio_common/shim/libc_shim.c ../audio_common/decoders/stb_vorbis.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
