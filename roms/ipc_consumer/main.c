#include "piolho.h"

static int step = 0;

int32_t update(void) {
    step++;

    if (step == 1) {
        // Test 1: Receive packet from ANY sender
        uint32_t buffer[4] = {0};
        int r = hear(ANY, buffer, sizeof(buffer), -1);
        if (r != OK) return ERROR;
        if (buffer[0] != 0xDEADBEEF || buffer[1] != 0x12345678 || buffer[2] != 0xCAFEBABE || buffer[3] != 0x42) {
            return ERROR;
        }

        // Test 2: Send reply back to producer
        uint32_t reply = 0x9999;
        r = tell("producer", &reply, sizeof(reply), 2000);
        if (r != OK) return ERROR;

        return DONE; // Success
    }

    return OK;
}
