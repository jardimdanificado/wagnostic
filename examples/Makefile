# Host build variables
CC = gcc
WASM3_DIR = runners/lib/wasm3
WASM3_SRC = $(WASM3_DIR)/m3_api_libc.c \
            $(WASM3_DIR)/m3_bind.c \
            $(WASM3_DIR)/m3_code.c \
            $(WASM3_DIR)/m3_compile.c \
            $(WASM3_DIR)/m3_core.c \
            $(WASM3_DIR)/m3_env.c \
            $(WASM3_DIR)/m3_exec.c \
            $(WASM3_DIR)/m3_function.c \
            $(WASM3_DIR)/m3_info.c \
            $(WASM3_DIR)/m3_module.c \
            $(WASM3_DIR)/m3_parse.c

HOST_CFLAGS = -I$(WASM3_DIR) -O2
HOST_LDFLAGS = -lSDL2 -lGL -lm
WASMTIME_DIR = runners/lib/wasmtime
WASMTIME_CFLAGS = -I$(WASMTIME_DIR)/include
WASMTIME_LDFLAGS = -L$(WASMTIME_DIR)/lib -lwasmtime -lpthread -ldl -lm

HOST_CFLAGS = -I$(WASM3_DIR) -O2
HOST_LDFLAGS = -lSDL2 -lGL -lm

TARGET_HOST = wagnostic
TARGET_HOST_GPU = wagnostic-gpu
TARGET_HOST_WASMTIME = wagnostic-wasmtime

# ROM build variables
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -Iinclude
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576
MEM_FLAGS = -Wl,--initial-memory=8388608

# Tools
IMG2FB = tools/img2fb.sh
AUDIO2PCM = tools/audio2pcm.py

TARGET_HOST_BENCH = wagnostic-bench
TARGET_HOST_BENCH_WASMTIME = wagnostic-bench-wasmtime
TARGET_HOST_V8 = wagnostic-v8
TARGET_HOST_BENCH_V8 = wagnostic-bench-v8
TARGET_HOST_BENCH_NODE = wagnostic-bench-node
TARGET_HOST_BENCH_SM = wagnostic-bench-sm
TARGET_HOST_BENCH_JS140 = wagnostic-bench-js140

.PHONY: all clean audio host host-gpu host-wasmtime host-bench host-bench-wasmtime host-v8 host-bench-v8 host-bench-node host-bench-sm host-bench-js140 term portmaster roms benchmarks benchmark_roms

# Default target: build host and ROMs
all: $(TARGET_HOST) roms

# Host targets
host: $(TARGET_HOST)

$(TARGET_HOST): runners/native/host.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ -lSDL2 -lm

host-gpu: $(TARGET_HOST_GPU)

$(TARGET_HOST_GPU): runners/native_gpu/host_gpu.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ $(HOST_LDFLAGS)

host-wasmtime: $(TARGET_HOST_WASMTIME)

$(TARGET_HOST_WASMTIME): runners/native-wasmtime/host.c
	$(CC) $(WASMTIME_CFLAGS) $< -o $@ $(HOST_LDFLAGS) $(WASMTIME_LDFLAGS)

host-bench: $(TARGET_HOST_BENCH)

$(TARGET_HOST_BENCH): runners/native/host_benchmark.c
	$(CC) $(HOST_CFLAGS) $< $(WASM3_SRC) -o $@ -lm

# wasmtime benchmark target
host-bench-wasmtime: $(TARGET_HOST_BENCH_WASMTIME)

$(TARGET_HOST_BENCH_WASMTIME): runners/native-wasmtime/host_benchmark.c
	$(CC) $(HOST_CFLAGS) $(WASMTIME_CFLAGS) $< -o $@ $(WASMTIME_LDFLAGS) $(HOST_LDFLAGS)

# V8 targets
CXX = g++
V8_DIR = runners/lib/libv8
V8_CXXFLAGS = -I$(V8_DIR)/include -std=c++20 -O2
V8_LDFLAGS = -L$(V8_DIR)/lib -Wl,-rpath,'$$ORIGIN/runners/lib/libv8/lib' \
             -lv8 -lv8_libplatform -lv8_libbase \
             -lpthread -ldl -lm -lrt

host-v8: $(TARGET_HOST_V8)

$(TARGET_HOST_V8): runners/native-v8/host_v8.cpp
	$(CXX) $(V8_CXXFLAGS) $< -o $@ $(HOST_LDFLAGS) $(V8_LDFLAGS)

host-bench-v8: $(TARGET_HOST_BENCH_V8)

$(TARGET_HOST_BENCH_V8): runners/native-v8/host_benchmark_v8.cpp
	$(CXX) $(V8_CXXFLAGS) $< -o $@ $(V8_LDFLAGS)

# Node.js benchmark target (no compilation — creates a wrapper script)
host-bench-node: $(TARGET_HOST_BENCH_NODE)

$(TARGET_HOST_BENCH_NODE): runners/native-node/host_benchmark_node.js
	printf '#!/usr/bin/env bash\nexec node "$$(dirname "$$0")/runners/native-node/host_benchmark_node.js" "$$@"\n' > $@
	chmod +x $@

# SpiderMonkey C++ benchmark target (native-spidermonkey, V8-style)
SM_CXXFLAGS = $(shell pkg-config --cflags mozjs-140)
SM_LDFLAGS = $(shell pkg-config --libs mozjs-140)

host-bench-sm: $(TARGET_HOST_BENCH_SM)

$(TARGET_HOST_BENCH_SM): runners/native-spidermonkey/host_benchmark_sm.cpp
	$(CXX) -std=c++17 $(SM_CXXFLAGS) $< -o $@ $(SM_LDFLAGS) -O2

# SpiderMonkey JS shell benchmark target (native-js140, Node.js-style)
host-bench-js140: $(TARGET_HOST_BENCH_JS140)

$(TARGET_HOST_BENCH_JS140): runners/native-js140/host_benchmark_js140.js
	printf '#!/usr/bin/env bash\nexec js140 "$$(dirname "$$0")/runners/native-js140/host_benchmark_js140.js" "$$@"\n' > $@
	chmod +x $@

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

benchmarks: benchmark_cpu.wasm benchmark_vram.wasm benchmark_audio.wasm \
	benchmark_particles.wasm benchmark_all.wasm

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

# Benchmark ROMs
benchmark_cpu.wasm: rom/benchmark_cpu.c
	$(CLANG) $(CFLAGS) $(MEM_FLAGS) $(LDFLAGS) rom/benchmark_cpu.c -o benchmark_cpu.wasm

benchmark_vram.wasm: rom/benchmark_vram.c
	$(CLANG) $(CFLAGS) -Wl,--initial-memory=67108864 $(LDFLAGS) rom/benchmark_vram.c -o benchmark_vram.wasm

benchmark_audio.wasm: rom/benchmark_audio.c
	$(CLANG) $(CFLAGS) -Wl,--initial-memory=33554432 $(LDFLAGS) rom/benchmark_audio.c -o benchmark_audio.wasm

benchmark_particles.wasm: rom/benchmark_particles.c
	$(CLANG) $(CFLAGS) -Wl,--initial-memory=33554432 $(LDFLAGS) rom/benchmark_particles.c -o benchmark_particles.wasm

benchmark_all.wasm: rom/benchmark_all.c
	$(CLANG) $(CFLAGS) -Wl,--initial-memory=67108864 $(LDFLAGS) rom/benchmark_all.c -o benchmark_all.wasm

clean:
	rm -f *.wasm $(TARGET_HOST) $(TARGET_HOST_GPU) $(TARGET_HOST_BENCH) $(TARGET_HOST_BENCH_WASMTIME) $(TARGET_HOST_V8) $(TARGET_HOST_BENCH_V8) $(TARGET_HOST_BENCH_NODE) $(TARGET_HOST_BENCH_SM) $(TARGET_HOST_BENCH_JS140) wagnostic-term wagnostic-bench-wamr wagnostic-bench-native wagnostic-bench-gpu wagnostic-wasmtime
	rm -f rom/draw/*.h rom/draw/*.raw
	rm -f rom/images/*.h rom/images/*.raw
	rm -f rom/audio/audio_data.h
