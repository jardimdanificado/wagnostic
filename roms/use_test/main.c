#include "piolho.h"
#include "clock.h"
#include "logger.h"
#include "comm_workers.h"

static clock_ext_t    *clock_ext;
static logger_t       *log_ext;
static int initialized = 0;
static int test_passed = 0;

int32_t update(void) {
    if (!initialized) {
        clock_ext = (clock_ext_t*)use("std:clock");
        log_ext   = (logger_t*)use("logger");
        int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
        void* unk = use("unknown_custom_xyz");

        test_passed = (clock_ext != NULL) &&
                      (log_ext != NULL) &&
                      (has_workers == 1) &&
                      (unk == NULL);

        initialized = 1;
    }

    return test_passed ? UPDATE_EXIT : UPDATE_ERROR;
}
