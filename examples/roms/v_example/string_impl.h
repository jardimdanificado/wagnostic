#pragma once
#include <stddef.h>
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}
int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}
void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}
void *memset(void *s, int c, size_t n) {
    char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (char)c;
    return s;
}
void *memmove(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}
