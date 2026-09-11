#include "piolho.h"

static int step = 0;

int32_t winit(void) {
    step = 0;
    return 0;
}

int32_t wupdate(void) {
    step++;

    if (step == 1) {
        // Test 1: Receive packet from ANY sender (WIPC_ANY / NULL) with infinite wait (-1)
        uint32_t buffer[4] = {0};
        int r = wask(WIPC_ANY, buffer, sizeof(buffer), -1);
        if (r != WIPC_OK) return WUPDATE_ERROR;
        if (buffer[0] != 0xDEADBEEF || buffer[1] != 0x12345678 || buffer[2] != 0xCAFEBABE || buffer[3] != 0x42) {
            return WUPDATE_ERROR;
        }

        // Test 2: Send reply back to producer
        uint32_t reply = 0x9999;
        r = wtell("producer", &reply, sizeof(reply), 2000);
        if (r != WIPC_OK) return WUPDATE_ERROR;

        return WUPDATE_EXIT; // Success
    }

    return WUPDATE_OK;
}

int32_t wexit(void) {
    return 0;
}
