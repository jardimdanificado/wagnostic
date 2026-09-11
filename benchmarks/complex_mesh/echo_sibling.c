#include "piolho.h"

int32_t update(void) {
    uint8_t buf[64];
    // Keep hear active
    hear(ANY, buf, sizeof(buf), 1);
    return OK;
}
