#include "piolho.h"
#include "comm_workers.h"

int32_t update(void) {
    int32_t has_workers = (int32_t)(uintptr_t)use("comm:workers");
    if (has_workers != 1) return ERROR;
    return DONE;
}
