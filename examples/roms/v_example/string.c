#include "string.h"
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
