ZIG ?= zig
ZIGFLAGS = build-exe -target wasm32-freestanding -fno-entry --export=wupdate --export=rom -OReleaseFast

TARGET = ../../zig_example.wasm

all: $(TARGET)

$(TARGET): main.zig
	$(ZIG) $(ZIGFLAGS) -femit-bin=$@ main.zig

clean:
	rm -f $(TARGET)

.PHONY: all clean
