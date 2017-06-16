#include <tracking.h>
#include <main.h>

void writePidOutput(mirrorOutput& out);

mirrorOutput lastPidOut;
adcSample lastAdcRead;
volatile unsigned samplesProcessed = 0;
volatile bool enteringTracking = false;

// Most significant bit first
void send32(uint32_t toSend) {
    uint16_t first = toSend >> 16;
    uint16_t second = toSend % (1 << 16);
    assert((((uint32_t) first) << 16) + second == toSend);
    SPI2.transfer16(first);
    SPI2.transfer16(second);
}

void trackingSetup() {
    // Begin dma transaction
}

void enterTracking() {
    enteringTracking = true; // Set this flag that we want to enter tracking mode, but the real setup work is done in firstLoopTracking()
    // This is because enterTracking() is called in an interrupt before the main loop is finished doing its thing
}

void firstLoopTracking() {
    debugPrintf("About to clear dma: offset is (this might be high) %d\n", dmaGetOffset());
    dmaStartSampling();
    assert(!dmaSampleReady());
    debugPrintf("Cleared dma: offset is (this should be <= 4) %d\n", dmaGetOffset());
    assert(!enteringTracking);
}

void leaveTracking() {
    enteringTracking = false;
}

void pidProcess(const volatile adcSample& s) {
    mirrorOutput out;
    lastPidOut.copy(out);
    writePidOutput(out);
    samplesProcessed++;
}

void writePidOutput(mirrorOutput& out) {
    send32(out.x_low);
    send32(out.x_high);
    send32(out.y_low);
    send32(out.y_high);
}

void taskTracking() {
    if (enteringTracking) {
        enteringTracking = false;
        firstLoopTracking();
    }
    assert(dmaGetOffset() < 5); // We should be able to keep up with data generation
    if (dmaGetOffset() >= 5) {
        debugPrintf("Offset is too high! It is %d\n", dmaGetOffset());
    }
    if (dmaSampleReady()) {
        volatile adcSample s = *dmaGetSample();
        pidProcess(s);
    }
}

void trackingHeartbeat() {
    char lastAdcReadBuf[40];
    char lastPidOutBuf[40];
    lastAdcRead.toString(lastAdcReadBuf, 40);
    lastPidOut.toString(lastPidOutBuf, 40);
    debugPrintf("Last pid output %s, last adc read %s, samples processed %d\n", lastPidOutBuf, lastAdcReadBuf, samplesProcessed);
}

void enterCalibration() {
    enterTracking();
}

void leaveCalibration() {
}

void taskCalibration() {
    taskTracking();
}

void calibrationHeartbeat() {
    trackingHeartbeat();
}
