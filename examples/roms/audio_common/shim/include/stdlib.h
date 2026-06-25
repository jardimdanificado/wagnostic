// stdlib.h — minimal libc shim for -nostdlib WASM builds
// Provides just enough for dr_wav / dr_mp3 / stb_vorbis to compile.
// All allocators are backed by a single static arena (see libc_shim.c).

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);
void *alloca(size_t size);   /* declared but never invoked: stb_vorbis
                               * wraps it in a short-circuit macro that
                               * is bypassed when we pass an alloc buffer */
void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
int   abs(int x);
void  abort(void);

#endif
