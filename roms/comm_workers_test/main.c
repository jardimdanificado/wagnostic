#include "piolho.h"
#include "comm_workers.h"

static int32_t has_workers = 0;

int32_t setup(void) {
    has_workers = (int32_t)(uintptr_t)use("comm:workers");
    if (has_workers != 1) return -1;
    return 0;
}

int32_t update(void) {
    if (has_workers != 1) return UPDATE_ERROR;
    return UPDATE_EXIT;
}
