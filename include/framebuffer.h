#ifndef PIOLHO_FRAMEBUFFER_H
#define PIOLHO_FRAMEBUFFER_H

#include <stdint.h>

#define FRAMEBUFFER_EXTENSION  "std:framebuffer"
#define WFRAMEBUFFER_EXTENSION "std:framebuffer"

typedef struct {
    uint32_t width;   /* Framebuffer width in pixels */
    uint32_t height;  /* Framebuffer height in pixels */
    uint32_t pixels;  /* WASM memory pointer to 32-bit RGBA pixel buffer */
} framebuffer_t;

typedef framebuffer_t wframebuffer_t;

#endif /* PIOLHO_FRAMEBUFFER_H */
