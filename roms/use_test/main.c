#include "piolho.h"
#include "clock.h"
#include "logger.h"
#include "comm_workers.h"

int32_t update(void) {
    clock_ext_t *clock_ext = (clock_ext_t*)use("clock");
    logger_t    *log_ext   = (logger_t*)use("logger");
    int32_t has_workers    = (int32_t)(uintptr_t)use("comm:workers");
    void* unk              = use("unknown_custom_xyz");

    int test_passed = (clock_ext != NULL) &&
                      (log_ext != NULL) &&
                      (has_workers == 1) &&
                      (unk == NULL);

    return test_passed ? DONE : ERROR;
}
