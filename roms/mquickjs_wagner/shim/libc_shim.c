// libc_shim.c — minimal libc for -nostdlib WASM builds.
// Bump allocator: each allocation has an 8-byte size header so
// realloc can copy the old contents.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HEADER_SIZE 8

extern unsigned char __heap_base;
static size_t arena_used = 0;

void *malloc(size_t size) {
    if (size == 0) return 0;
    size_t total = ((size + HEADER_SIZE) + 7) & ~(size_t)7;
    size_t mem_size = __builtin_wasm_memory_size(0) * 65536;
    char *heap_start = (char *)&__heap_base;
    if ((size_t)(heap_start + arena_used + total) > mem_size) {
        size_t needed = (size_t)(heap_start + arena_used + total) - mem_size;
        size_t pages = (needed + 65535) / 65536;
        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1) return 0;
    }
    char *base = heap_start + arena_used;
    *(size_t*)base = size;
    arena_used += total;
    return base + HEADER_SIZE;
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
    size_t old_size = *(size_t*)((char*)ptr - HEADER_SIZE);
    void *p = malloc(size);
    if (!p) return 0;
    size_t copy = old_size < size ? old_size : size;
    memcpy(p, ptr, copy);
    return p;
}

void free(void *ptr) {
    if (!ptr) return;
    char *base = (char *)ptr - HEADER_SIZE;
    size_t size = *(size_t *)base;
    size_t total = ((size + HEADER_SIZE) + 7) & ~(size_t)7;
    char *heap_start = (char *)&__heap_base;
    if (base + total == heap_start + arena_used) {
        arena_used -= total;
    }
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

// Math functions
#define M_PI  3.14159265358979323846
#define M_LN2 0.69314718055994530942

double fabs(double x)  { return x < 0 ? -x : x; }
double floor(double x) {
    int64_t i = (int64_t)x;
    if ((double)i > x) i--;
    return (double)i;
}
double ceil(double x) {
    int64_t i = (int64_t)x;
    if ((double)i < x) i++;
    return (double)i;
}
double ldexp(double x, int n) {
    if (n == 0) return x;
    double r = 1.0;
    if (n > 0) while (n-- > 0) r *= 2.0;
    else       while (n++ < 0) r *= 0.5;
    return x * r;
}
double sin(double x) {
    while (x >  M_PI) x -= 2.0 * M_PI;
    while (x < -M_PI) x += 2.0 * M_PI;
    if (x < 0) return -sin(-x);
    if (x > M_PI / 2.0) x = M_PI - x;
    double y = x * (M_PI - x);
    return 16.0 * y / (5.0 * M_PI * M_PI - 4.0 * y);
}
double cos(double x) { return sin(x + M_PI / 2.0); }
double exp(double x) {
    double y = x / M_LN2;
    int    k = (int)(y + (y < 0 ? -0.5 : 0.5));
    double f = y - (double)k;
    double u = f * M_LN2;
    double p = 1.0 + u * (1.0 + u * (0.5 + u * (1.0/6.0 + u * (1.0/24.0))));
    while (k >  0) { p *= 2.0; k--; }
    while (k <  0) { p *= 0.5; k++; }
    return p;
}
double log(double x) {
    if (x <= 0) return -1e300;
    int k = 0;
    while (x >= 2.0) { x *= 0.5; k++; }
    while (x <  0.5) { x *= 2.0; k--; }
    double u = (x - 1.0) / (x + 1.0);
    double u2 = u * u;
    double s = u * (1.0 + u2 * (1.0/3.0 + u2 * (1.0/5.0 + u2 * (1.0/7.0))));
    return 2.0 * s + (double)k * M_LN2;
}
double pow(double x, double y) {
    if (x == 0.0) return 0.0;
    if (y == 0.0) return 1.0;
    if (x < 0) {
        int yi = (int)y;
        if ((double)yi == y) {
            double r = exp(y * log(-x));
            return (yi & 1) ? -r : r;
        }
        return 0.0;
    }
    return exp(y * log(x));
}
double sqrt(double x) {
    if (x <= 0) return 0;
    double g = x * 0.5;
    for (int i = 0; i < 16; i++) g = (g + x / g) * 0.5;
    return g;
}

float fabsf(float x) { return x < 0 ? -x : x; }
float floorf(float x) { return (float)floor((double)x); }
float ceilf(float x)  { return (float)ceil((double)x); }
float ldexpf(float x, int n) { return (float)ldexp((double)x, n); }
float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float expf(float x) { return (float)exp((double)x); }
float logf(float x) { return (float)log((double)x); }
float powf(float x, float y) { return (float)pow((double)x, (double)y); }
float sqrtf(float x) { return (float)sqrt((double)x); }

__attribute__((weak)) void preload(void) {}
__attribute__((weak)) void setup(void) {}
__attribute__((weak)) void draw(void) {}
