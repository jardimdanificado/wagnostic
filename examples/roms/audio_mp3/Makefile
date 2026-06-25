CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=8388608

TARGET  = ../../audio_mp3.wasm
HEADER  = data/audio.h
DATA    = data/audio.bin
SHIM    = ../audio_common/shim/libc_shim.c
SHIM_INC = ../audio_common/shim/include
DECODERS = ../audio_common/decoders
GEN     = ../audio_common/gen_audio.sh

all: $(TARGET)

$(DATA) $(HEADER): $(GEN)
	$(GEN) mp3 $(HEADER) $(DATA)

$(TARGET): main.c $(HEADER) $(SHIM) $(GEN)
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) \
		-I$(DECODERS) -I$(SHIM_INC) -Idata \
		-o $@ main.c $(SHIM)

clean:
	rm -f $(TARGET) $(HEADER) $(DATA)

.PHONY: all clean
