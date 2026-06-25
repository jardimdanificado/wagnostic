// libc_shim.c — minimal libc implementation for -nostdlib WASM builds
//
// Provides malloc/free/realloc/memcpy/etc. backed by a single static
// arena. Used to let dr_wav, dr_mp3, and stb_vorbis link cleanly
// under -nostdlib. The arena is large enough (1MB) for typical
// short audio clips; the bump allocator is monotonic — free() is
// a no-op since the audio libraries only allocate during init.

#include <stddef.h>
#include <stdint.h>

#define ARENA_SIZE (1024 * 1024)
static char arena[ARENA_SIZE];
static size_t arena_used = 0;

void *malloc(size_t size) {
    if (size == 0) return 0;
    size = (size + 7) & ~(size_t)7;
    if (arena_used + size > ARENA_SIZE) return 0;
    void *p = &arena[arena_used];
    arena_used += size;
    return p;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        char *cp = (char *)p;
        for (size_t i = 0; i < total; i++) cp[i] = 0;
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) return 0;
    void *p = malloc(size);
    if (!p) return 0;
    return p;
}

void free(void *ptr) {
    (void)ptr;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    char *d = (char *)dest;
    const char *s = (const char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    char *p = (char *)s;
    for (size_t i = 0; i < n; i++) p[i] = (char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const char *a = (const char *)s1;
    const char *b = (const char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) { dest[i] = src[i]; i++; }
    while (i < n) dest[i++] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i])
            return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
    char *arr = (char *)base;
    if (nmemb < 2) return;
    for (size_t i = 1; i < nmemb; i++) {
        char tmp[64];
        if (size > sizeof(tmp)) return;
        size_t j = i;
        memcpy(tmp, arr + i * size, size);
        while (j > 0 && compar(arr + (j - 1) * size, tmp) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }
        memcpy(arr + j * size, tmp, size);
    }
}

void abort(void) {
    for (;;) (void)0;
}

int abs(int x) {
    return x < 0 ? -x : x;
}
