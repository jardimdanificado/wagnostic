#include "piolho.h"

static int step = 0;

int32_t setup(void) {
    step = 0;
    return 0;
}

int32_t update(void) {
    step++;

    if (step == 1) {
        // Test 1: Receive packet from ANY sender (ANY / NULL) with infinite wait (-1)
        uint32_t buffer[4] = {0};
        int r = hear(ANY, buffer, sizeof(buffer), -1);
        if (r != IPC_OK) return UPDATE_ERROR;
        if (buffer[0] != 0xDEADBEEF || buffer[1] != 0x12345678 || buffer[2] != 0xCAFEBABE || buffer[3] != 0x42) {
            return UPDATE_ERROR;
        }

        // Test 2: Send reply back to producer
        uint32_t reply = 0x9999;
        r = tell("producer", &reply, sizeof(reply), 2000);
        if (r != IPC_OK) return UPDATE_ERROR;

        return UPDATE_EXIT; // Success
    }

    return UPDATE_OK;
}

int32_t shutdown(void) {
    return 0;
}
