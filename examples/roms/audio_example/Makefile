CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined
MEM_FLAGS = -Wl,--initial-memory=67108864
AUDIO2PCM = ../../tools/audio2pcm.py

TARGET = ../../audio_example.wasm

all: $(TARGET)

audio_data.h: audio.mp3
	$(AUDIO2PCM) audio.mp3 audio_data.h --name music

$(TARGET): main.c audio_data.h
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) -o $@ main.c

clean:
	rm -f $(TARGET) audio_data.h

.PHONY: all clean
