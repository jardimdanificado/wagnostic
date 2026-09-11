// logger_example — Demonstrates requesting and writing to a custom "logger" extension

#include "piolho.h"
#include "logger.h"

static logger_t *logger;
static int step = 0;

static void log_str(const char *s) {
    if (!logger || !logger->buffer || logger->capacity == 0) return;
    char *dst = (char*)logger->buffer;
    uint32_t len = 0;
    while (s[len] && len < logger->capacity - 1) {
        dst[len] = s[len];
        len++;
    }
    dst[len] = '\0';
    logger->length = len;
}

int32_t update(void) {
    if (!logger) {
        logger = (logger_t*)use(LOGGER_EXTENSION);
    }

    step++;

    if (step == 1) {
        log_str("Hello from Piolho 2.0 ROM!");
    } else if (step == 2) {
        log_str("Step 2: Custom extensions are working smoothly.");
    } else if (step == 3) {
        log_str("Step 3: Completing execution. Goodbye!");
        return UPDATE_EXIT;
    }

    return UPDATE_OK;
}
