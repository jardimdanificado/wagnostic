#include "piolho.h"
#include "logger.h"

static logger_t *log_ext = 0;
static int step = 0;

int32_t update(void) {
    if (!log_ext) {
        log_ext = (logger_t*)use("logger");
    }

    step++;

    if (log_ext) {
        if (step == 1) {
            logger_print(log_ext, "Hello from Piolho ROM!");
        } else if (step == 2) {
            logger_print(log_ext, "Step 2: Custom extensions are working smoothly.");
        } else if (step == 3) {
            logger_print(log_ext, "Step 3: Completing execution. Goodbye!");
            return DONE;
        }
    }

    return (step > 5) ? DONE : OK;
}
