#include "piolho.h"

int32_t update(void) {
    uint32_t buf;
    hear(ANY, &buf, sizeof(buf), 10);
    return OK;
}
