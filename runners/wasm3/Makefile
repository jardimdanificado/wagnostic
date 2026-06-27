CC = gcc
WASM3_DIR = wasm3/source

# MinGW detection for Windows
ifneq (,$(findstring MINGW,$(shell uname -s)))
  LDFLAGS = -lmingw32 -lSDL2main -lSDL2 -lopengl32 -lm
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    # Homebrew paths: Intel = /usr/local, ARM = /opt/homebrew
    PKG_CONFIG_PATH := $(shell find /opt/homebrew/lib/pkgconfig /usr/local/lib/pkgconfig /usr/local/opt/sdl2/lib/pkgconfig -name "sdl2.pc" 2>/dev/null | head -1 | xargs dirname 2>/dev/null)
    ifneq ($(PKG_CONFIG_PATH),)
      CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags sdl2)
      LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs sdl2) -framework OpenGL -lm
    else
      # Fallback: use brew --prefix
      SDL2_PREFIX := $(shell brew --prefix sdl2 2>/dev/null)
      ifneq ($(SDL2_PREFIX),)
        CFLAGS += -I$(SDL2_PREFIX)/include
        LDFLAGS = -L$(SDL2_PREFIX)/lib -lSDL2 -framework OpenGL -lm
      else
        LDFLAGS = -lSDL2 -framework OpenGL -lm
      endif
    endif
  else
    LDFLAGS = -lSDL2 -lGL -lm
  endif
endif
WASM3_REPO = https://github.com/wasm3/wasm3.git
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
CFLAGS = -I$(WASM3_DIR) -O2

HOST = ../../wagnostic

.PHONY: all host clean

all: host

# Auto-clone wasm3 if missing
$(WASM3_DIR)/m3_api_libc.c:
	@echo "Cloning wasm3 from $(WASM3_REPO)..."
	git clone --depth 1 $(WASM3_REPO) wasm3

host: $(HOST)

$(HOST): host.c $(WASM3_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(HOST)
