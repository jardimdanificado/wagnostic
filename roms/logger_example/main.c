// logger_example — Minimalist Wagnostic 2.0 ROM demonstrating custom extension
#include "wagnostic.h"

typedef struct {
    uint32_t buffer;
    uint32_t capacity;
    uint32_t length;
} wlogger_t;

static wlogger_t *logger = NULL;
static int step = 0;

static void log_str(const char *str) {
    if (!logger || !logger->buffer || !logger->capacity) return;
    char *buf = (char*)logger->buffer;
    int i = 0;
    while (str[i] && i < (int)logger->capacity - 1) {
        buf[i] = str[i];
        i++;
    }
    logger->length = i;
}

int32_t update(void) {
    if (!logger) {
        logger = (wlogger_t*)ask("logger");
    }

    step++;

    if (step == 1) {
        log_str("Hello from Wagnostic 2.0 ROM!");
        return UPDATE_OK;
    } else if (step == 2) {
        log_str("Step 2: Custom extensions are working smoothly.");
        return UPDATE_OK;
    } else {
        log_str("Step 3: Completing execution. Goodbye!");
        return UPDATE_EXIT;
    }
}
