CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=33554432

TARGET = ../../benchmark_particles.wasm

all: $(TARGET)

$(TARGET): main.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
