#include "piolho.h"

static int step = 0;

int32_t winit(void) {
    step = 0;
    return 0;
}

int32_t wupdate(void) {
    step++;

    if (step == 1) {
        // Test 1: Self-communication should fail with WIPC_PARAM (-3)
        uint32_t dummy = 123;
        int r = wtell("producer", &dummy, sizeof(dummy), 0);
        if (r != WIPC_PARAM) return WUPDATE_ERROR;

        // Test 2: Timeout on non-existent target
        r = wtell("non_existent_worker", &dummy, sizeof(dummy), 20);
        if (r != WIPC_TIMEOUT && r != WIPC_TARGET) return WUPDATE_ERROR;

        // Test 3: Send packet to consumer with blocking timeout 2000ms
        uint32_t payload[4] = {0xDEADBEEF, 0x12345678, 0xCAFEBABE, 0x42};
        r = wtell("consumer", payload, sizeof(payload), 2000);
        if (r != WIPC_OK) return WUPDATE_ERROR;
    }

    if (step == 2) {
        // Test 4: Wait for consumer reply
        uint32_t reply = 0;
        int r = wask("consumer", &reply, sizeof(reply), 2000);
        if (r != WIPC_OK || reply != 0x9999) return WUPDATE_ERROR;

        return WUPDATE_EXIT; // Success
    }

    return WUPDATE_OK;
}

int32_t wexit(void) {
    return 0;
}
