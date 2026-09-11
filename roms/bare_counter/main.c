#include "piolho.h"

static int counter = 0;

int32_t update(void) {
    counter++;
    if (counter >= 10) {
        return DONE;
    }
    return OK;
}
