#include "piolho.h"

static int count = 0;
int32_t update(void) {
    uint32_t payload = ++count;
    tell("consumer", &payload, sizeof(payload), 10);
    return OK;
}
