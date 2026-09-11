// bare_counter — Minimalist Piolho 2.0 ROM with zero extensions
// Runs a 10-step compute loop and requests clean termination.

#include "piolho.h"

static int step_count = 0;
static int fib_a = 0;
static int fib_b = 1;

int32_t wupdate(void) {
    step_count++;

    // Compute Fibonacci number
    int next = fib_a + fib_b;
    fib_a = fib_b;
    fib_b = next;

    // After 10 steps, terminate cleanly
    if (step_count >= 10) {
        return WUPDATE_EXIT;
    }

    return WUPDATE_OK;
}
