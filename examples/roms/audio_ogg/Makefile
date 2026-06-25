CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w \
         -DSTB_VORBIS_NO_STDIO
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=8388608

TARGET  = ../../audio_ogg.wasm
HEADER  = data/audio.h
DATA    = data/audio.bin
SHIM    = ../audio_common/shim/libc_shim.c
VORBIS  = ../audio_common/decoders/stb_vorbis.c
SHIM_INC = ../audio_common/shim/include
DECODERS = ../audio_common/decoders
GEN     = ../audio_common/gen_audio.sh

all: $(TARGET)

$(DATA) $(HEADER): $(GEN)
	$(GEN) ogg $(HEADER) $(DATA)

$(TARGET): main.c $(HEADER) $(SHIM) $(VORBIS) $(GEN)
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) \
		-I$(DECODERS) -I$(SHIM_INC) -Idata \
		-o $@ main.c $(SHIM) $(VORBIS)

clean:
	rm -f $(TARGET) $(HEADER) $(DATA)

.PHONY: all clean
