# Host build variables
CC = gcc
WASM3_DIR = runners/lib/wasm3
WASM3_SRC = $(WASM3_DIR)/*.c

HOST_CFLAGS = -I$(WASM3_DIR) -O2
HOST_LDFLAGS = -lSDL2 -lGL -lm

TARGET_HOST = wagnostic

# ROM build variables
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -Iinclude
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=8388608

# Tools
IMG2FB = tools/img2fb.sh
AUDIO2PCM = tools/audio2pcm.py

.PHONY: all clean audio host term portmaster

# Default target: build host and ROMs
all: $(TARGET_HOST) roms

# Host targets
host: $(TARGET_HOST)

$(TARGET_HOST): runners/native/host.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ $(HOST_LDFLAGS)

term: runners/native/ncurses_host.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o wagnostic-term -lncurses -lm

CC_AARCH64 = aarch64-linux-gnu-gcc
CFLAGS_AARCH64 = -Irunners/lib/wasm3 -O2 -DPORTMASTER
LDFLAGS_AARCH64 = -lSDL2 -lGLESv2 -lm

portmaster:
	mkdir -p wagnostic
	@echo "Montando ambiente de Cross-Compile no Docker..."
	@sudo docker run --rm -e DEBIAN_FRONTEND=noninteractive -v $(PWD):/bld -w /bld ubuntu:20.04 bash -c " \
		sed -i 's/^deb /deb [arch=amd64] /g' /etc/apt/sources.list && \
		echo 'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal main restricted universe multiverse' >> /etc/apt/sources.list && \
		echo 'deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ focal-updates main restricted universe multiverse' >> /etc/apt/sources.list && \
		dpkg --add-architecture arm64 && apt-get update -qq && \
		apt-get install -yq --no-install-recommends gcc-aarch64-linux-gnu libsdl2-dev:arm64 libgles2-mesa-dev:arm64 && \
		$(CC_AARCH64) $(CFLAGS_AARCH64) runners/native/host.c $(WASM3_SRC) -o wagnostic/$(TARGET_HOST) $(LDFLAGS_AARCH64) \
	"
	@echo "PortMaster build complete! Files are inside wagnostic/ and wagnostic.sh"
	@echo "ZIP the files: zip -r wagnostic_port.zip wagnostic wagnostic.sh"

# ROM targets
roms: buttons_test.wasm test_8bpp.wasm test_16bpp.wasm test_32bpp.wasm \
	draw_example.wasm images_example.wasm roguelike_example.wasm tracker_example.wasm \
	fallback_test.wasm terminal_test.wasm

audio: audio_test.wasm audio_example.wasm

# Data Generation Rules
rom/draw/image.h: rom/images/image.webp
	$(IMG2FB) rom/images/image.webp rom/draw/image 16

rom/images/image.h: rom/images/image.webp
	$(IMG2FB) rom/images/image.webp rom/images/image 16

rom/images/image8.h: rom/images/image.webp
	$(IMG2FB) rom/images/image.webp rom/images/image8 8

rom/images/image16.h: rom/images/image.webp
	$(IMG2FB) rom/images/image.webp rom/images/image16 16

rom/images/image32.h: rom/images/image.webp
	$(IMG2FB) rom/images/image.webp rom/images/image32 32

rom/audio/audio_data.h: rom/audio/audio.mp3
	$(AUDIO2PCM) rom/audio/audio.mp3 rom/audio/audio_data.h --name music

# WASM Rules
audio_example.wasm: rom/audio/main.c rom/audio/audio_data.h
	$(CLANG) $(CFLAGS) -Irom/audio -Wl,--initial-memory=67108864 $(LDFLAGS) rom/audio/main.c -o audio_example.wasm

buttons_test.wasm: rom/buttons_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/buttons_test.c -o buttons_test.wasm

test_8bpp.wasm: rom/test_8bpp.c rom/images/image8.h
	$(CLANG) $(CFLAGS) -Irom/images $(MEM_FLAGS) $(LDFLAGS) rom/test_8bpp.c -o test_8bpp.wasm

test_16bpp.wasm: rom/test_16bpp.c rom/images/image16.h
	$(CLANG) $(CFLAGS) -Irom/images $(MEM_FLAGS) $(LDFLAGS) rom/test_16bpp.c -o test_16bpp.wasm

test_32bpp.wasm: rom/test_32bpp.c rom/images/image32.h
	$(CLANG) $(CFLAGS) -Irom/images $(MEM_FLAGS) $(LDFLAGS) rom/test_32bpp.c -o test_32bpp.wasm

audio_test.wasm: rom/audio_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/audio_test.c -o audio_test.wasm

draw_example.wasm: rom/draw/main.c rom/draw/image.h
	$(CLANG) $(CFLAGS) -Irom/draw $(MEM_FLAGS) $(LDFLAGS) rom/draw/main.c -o draw_example.wasm

images_example.wasm: rom/images/main.c rom/images/image.h
	$(CLANG) $(CFLAGS) -Irom/images $(MEM_FLAGS) $(LDFLAGS) rom/images/main.c -o images_example.wasm

roguelike_example.wasm: rom/roguelike/main.c rom/roguelike/data/sprites.h
	$(CLANG) $(CFLAGS) -Irom/roguelike $(MEM_FLAGS) $(LDFLAGS) rom/roguelike/main.c -o roguelike_example.wasm

tracker_example.wasm: rom/tracker/main.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/tracker/main.c -o tracker_example.wasm

fallback_test.wasm: rom/fallback_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/fallback_test.c -o fallback_test.wasm

terminal_test.wasm: rom/terminal_test.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/terminal_test.c -o terminal_test.wasm

clean:
	rm -f *.wasm $(TARGET_HOST) wagnostic-term
	rm -f rom/draw/*.h rom/draw/*.raw
	rm -f rom/images/*.h rom/images/*.raw
	rm -f rom/audio/audio_data.h
