ASC ?= asc
ASCFLAGS = --use abort=main/abort --outFile

TARGET = ../as_example.wasm

all: $(TARGET)

$(TARGET): main.ts
	$(ASC) main.ts $(ASCFLAGS) $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
