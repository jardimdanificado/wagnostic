#ifndef PIOLHO_FRAMEBUFFER_H
#define PIOLHO_FRAMEBUFFER_H

#include <stdint.h>

#define WFRAMEBUFFER_EXTENSION "std:framebuffer"
#define WSURFACE_EXTENSION     "std:framebuffer"

typedef struct {
    uint32_t width;         /* Framebuffer width in pixels */
    uint32_t height;        /* Framebuffer height in pixels */
    uint32_t pixels;        /* WASM memory pointer to 32-bit RGBA8888 pixel buffer (uint32_t[width * height]) */
} wframebuffer_t;

typedef wframebuffer_t wsurface_t;

#endif
