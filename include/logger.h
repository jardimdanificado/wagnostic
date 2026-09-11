#ifndef PIOLHO_LOGGER_H
#define PIOLHO_LOGGER_H

#include <stdint.h>

#define LOGGER_EXTENSION  "logger"

typedef struct {
    uint32_t buffer;      /* WASM memory pointer to UTF-8 buffer */
    uint32_t capacity;    /* Buffer capacity */
    uint32_t length;      /* Number of bytes written by ROM */
} logger_t;

static inline void logger_print(logger_t *log, const char *str) {
    if (!log || !log->buffer || !str) return;
    char *buf = (char*)(uintptr_t)log->buffer;
    uint32_t i = 0;
    while (str[i] != '\0' && i < log->capacity) {
        buf[i] = str[i];
        i++;
    }
    log->length = i;
}

#endif /* PIOLHO_LOGGER_H */
