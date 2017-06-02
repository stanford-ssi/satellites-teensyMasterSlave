#include <main.h>

extern uint32_t backOfBuffer;

void testDma() {
    assert(backOfBuffer == 0);
    assert(!dmaSampleReady());
}

void testBreakingLoop() {
    unsigned int startTimeCheck = micro;
    for (int i = 0; !(SPI2_SR & SPI_SR_TCF); i++) {
        if (i > 10000) {
            debugPrintf("Note: loop broken\n"); // Test guaranteed termination on loop
            break;
        }
    } // About 800 micros
    unsigned int timeSpent = micro - startTimeCheck;
    debugPrintf("Time spent: %d\n", (unsigned int) timeSpent);
    assert(timeSpent > 100);
}

void runTests() {
    if (DEBUG) {
        testDma();
        testBreakingLoop();
    }
}
