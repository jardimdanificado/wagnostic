#ifndef PIOLHO_LOGGER_H
#define PIOLHO_LOGGER_H

#include <stdint.h>

#define WLOGGER_EXTENSION "logger"

typedef struct {
    uint32_t buffer;      /* WASM memory pointer to UTF-8 text buffer */
    uint32_t capacity;    /* Buffer capacity in bytes */
    uint32_t length;      /* Length of text written by ROM (host clears to 0 after printing) */
} wlogger_t;

#endif /* PIOLHO_LOGGER_H */
