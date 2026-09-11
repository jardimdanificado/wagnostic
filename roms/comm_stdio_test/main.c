#include "piolho.h"
#include "comm_stdio.h"

static comm_stdio_t *stdio_ext = 0;
static int step = 0;

int32_t update(void) {
    if (!stdio_ext) {
        stdio_ext = (comm_stdio_t*)ask("comm:stdio");
        if (!stdio_ext) return ERROR;
    }

    if (step == 0) {
        comm_stdio_print("Hello from WASM stdio!\n");
        comm_stdio_eprint("Error log from WASM stdio!\n");
        step = 1;
        return OK;
    }

    return DONE;
}
