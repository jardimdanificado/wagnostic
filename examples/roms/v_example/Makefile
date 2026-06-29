V ?= v
VFLAGS = -os wasm32 -enable-globals

TARGET = ../../v_example.wasm

all: $(TARGET)

$(TARGET): main.v
	$(V) $(VFLAGS) -cflags "-I." -o $@ main.v

clean:
	rm -f $(TARGET)

.PHONY: all clean
