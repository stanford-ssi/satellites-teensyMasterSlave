#include <main.h>
#include <spiMaster.h>

extern QuadCell quadCell;
uint64_t test = 0xbeefab10;

void swapUint(uint32_t &axis) {
    uint16_t temp = axis % (1 << 16);
    axis = (axis / (1 << 16)) + (temp << 16);
}

void testSwap() {
    uint32_t blah = 0xbeefabcd;
    int32_t blah2 = blah;
    swapUint(blah);
    swapUint((uint32_t &) blah2);
    assert(blah == (uint32_t) blah2);
    assert(blah == 0xabcdbeef);
    debugPrintf("Blah %u %d %u\n", blah, blah2, blah2);
}

void testDma() {
    quadCell.adcStartSampling();
    assert(!quadCell.adcSampleReady());
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
    assert(testInt == -1 * testInt);
    (void) testInt;
    assert(test == (uint32_t) testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    test = (1 << 31) + (1 << 30);
    testInt = test;
    (void) testInt;
    assert(test == (uint32_t) testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    test = (1 << 30);
    testInt = test;
    (void) testInt;
    assert(test == (uint32_t) testInt);
    debugPrintf("Unsigned %u, signed %d\n", test, testInt);

    volatile uint64_t test64 = (1 << 31);
    int64_t testInt64 = test64;
    (void) testInt64;
    assert(test64 == (uint64_t) testInt64);
    debugPrintf("Unsigned %u, signed %d\n", test64, testInt64);
}

void runTests() {
    if (DEBUG) {
        debugPrintf("Errors at %p\n", &errors);
        debugPrintf("Test at %p\n", &test);
        testSwap();
        testDma();
        //testBreakingLoop();
        testInteger();
    }
}
