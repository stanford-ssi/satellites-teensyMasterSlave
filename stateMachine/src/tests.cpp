#include <main.h>

extern uint32_t backOfBuffer;

void testDma() {
    assert(backOfBuffer == 0);
    //assert(!dmaSampleReady());
}

void testBreakingLoop() {
    debugPrintf("Testing loop breaks\n");
    unsigned int startTimeCheck = micros();
    for (int i = 0; !(SPI2_SR & SPI_SR_TCF); i++) {
        if (i > 10000) {
            debugPrintf("Note: loop broken\n"); // Test guaranteed termination on loop
            break;
        }
    } // About 800 micros
    unsigned int timeSpent = micros() - startTimeCheck;
    debugPrintf("Time spent: %d\n", (unsigned int) timeSpent);
    assert(timeSpent > 100);
}

void testInteger() {
    volatile uint32_t test = (1 << 31);
    int32_t testInt = test;
    (void) testInt;
    assert(test == testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    test = (1 << 31) + (1 << 30);
    testInt = test;
    (void) testInt;
    assert(test == testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    test = (1 << 30);
    testInt = test;
    (void) testInt;
    assert(test == testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    volatile uint64_t test64 = (1 << 31);
    int64_t testInt64 = test64;
    (void) testInt64;
    assert(test64 == testInt64);
    debugPrintf("Unsigned %u, signed %d\n", test64, testInt64);
}

void runTests() {
    if (DEBUG) {
        testDma();
        testBreakingLoop();
        testInteger();
    }
}
