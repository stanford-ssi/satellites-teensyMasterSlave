#include <tracking.h>
#include <main.h>

void writePidOutput();

volatile int lastPidOut = -1;
volatile int lastAdcRead = -1;
volatile unsigned samplesProcessed = 0;

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
    dmaStartSampling();
}

void leaveTracking() {
}

void pidProcess(const volatile adcSample& s) {
    writePidOutput();
    samplesProcessed++;
}

void writePidOutput() {
    mirrorOutput out;
    send32(out.x_low);
    send32(out.x_high);
    send32(out.y_low);
    send32(out.y_high);
}

void taskTracking() {
    assert(dmaGetOffset() < 5); // We should be able to keep up with data generation
    if (dmaSampleReady()) {
        volatile adcSample s = *dmaGetSample();
        pidProcess(s);
    }
}

void trackingHeartbeat() {
    debugPrintf("Last pid output %d, last adc read %d, samples processed\n", lastPidOut, lastAdcRead, samplesProcessed);
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
