
TARGET_WASM = game.wasm
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -I.
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576 -Wl,--initial-memory=8388608

.PHONY: all clean

all: $(TARGET_WASM)

$(TARGET_WASM): main.c olive.c image_data.h
	$(CLANG) $(CFLAGS) $(LDFLAGS) main.c -o $(TARGET_WASM)

clean:
	rm -f $(TARGET_WASM)
