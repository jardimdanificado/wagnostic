#ifndef PIOLHO_LOGGER_H
#define PIOLHO_LOGGER_H

#include <stdint.h>

#define LOGGER_EXTENSION  "logger"
#define WLOGGER_EXTENSION "logger"

typedef struct {
    uint32_t buffer;      /* WASM memory pointer to UTF-8 buffer */
    uint32_t capacity;    /* Buffer capacity */
    uint32_t length;      /* Number of bytes written by ROM */
} logger_t;

typedef logger_t wlogger_t;

#endif /* PIOLHO_LOGGER_H */
