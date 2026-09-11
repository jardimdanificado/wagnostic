#include "piolho.h"

static int step = 0;

int32_t update(void) {
    step++;

    if (step == 1) {
        // Test 1: Self-communication should fail with ERROR_PARAM (-3)
        uint32_t dummy = 123;
        int r = tell("producer", &dummy, sizeof(dummy), 0);
        if (r != ERROR_PARAM) return ERROR;

        // Test 2: Target not found or timeout
        r = tell("non_existent_worker", &dummy, sizeof(dummy), 20);
        if (r != TIMEOUT && r != ERROR_TARGET) return ERROR;

        // Test 3: Send packet to consumer with blocking timeout
        uint32_t payload[4] = {0xDEADBEEF, 0x12345678, 0xCAFEBABE, 0x42};
        r = tell("consumer", payload, sizeof(payload), 2000);
        if (r != OK) return ERROR;
    }

    if (step == 2) {
        // Test 4: Wait for consumer reply
        uint32_t reply = 0;
        int r = hear("consumer", &reply, sizeof(reply), 2000);
        if (r != OK || reply != 0x9999) return ERROR;

        return DONE; // Success
    }

    return OK;
}
