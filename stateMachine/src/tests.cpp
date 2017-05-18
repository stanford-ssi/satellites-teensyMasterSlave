#include <main.h>

void testDmaStop() {
    if (!DMASPI0.stopped())
    {
      DMASPI0.stop();
    }
    assert(DMASPI0.stopped());
    debugPrintf("Booting up dma\n");
    DMASPI0.start();
    trx = DmaSpi::Transfer((const unsigned uint8_t *) nullptr, IMU_BUFFER_SIZE / 100, imuSamples, 0xff);
    DMASPI0.registerTransfer(trx);
    while (trx.busy()) {
    }
    assert(trx.done());
    DMASPI0.stop();
    assert(DMASPI0.stopped());
    for (int i = 0; i < 5; i++) {
        DMASPI0.start();
        Serial.println("DMA started!\n");
        trx = DmaSpi::Transfer((const unsigned uint8_t *) nullptr, IMU_BUFFER_SIZE, imuSamples, 0xff);
        DMASPI0.registerTransfer(trx);
        delay(1);
        debugPrintf("DMA state before stop: %d\n", DMASPI0.state_);
        DMASPI0.stop();
        debugPrintf("DMA state: %d\n", DMASPI0.state_);
        assert(DMASPI0.stopped());
        assert(trx.busy());
        assert(trx.m_state == DmaSpi::Transfer::error);
    }
    DMASPI0.start();
    assert(DMASPI0.stopped());
}

void testBreakingLoop() {
    unsigned int startTimeCheck = micro;
    for (int i = 0; !(SPI2_SR & SPI_SR_TCF); i++) {
        if (i > 100000) {
            debugPrintf("Note: loop broken\n"); // Test guaranteed termination on loop
            break;
        }
    }
    unsigned int timeSpent = micro - startTimeCheck;
    debugPrintf("Time spent: %d\n", (unsigned int) timeSpent);
    assert(timeSpent > 500);
}

void runTests() {
    if (DEBUG) {
        testDmaStop();
        testBreakingLoop();
    }
}
