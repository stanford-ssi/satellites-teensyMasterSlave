#include "packet.h"
#include "main.h"
#include "states.h"

IntervalTimer timer;
volatile uint16_t state = SETUP_STATE;
elapsedMicros micro = 0;
volatile unsigned int timeAlive = 0;
volatile int errors = 0;
volatile int bugs = 0;
volatile unsigned int lastLoopTime = 0;
volatile unsigned int lastLoopState = state;
volatile unsigned int lastAnalogRead = 0;
volatile unsigned int completedDmaTransfers = 0;
volatile unsigned int numIdles = 0; // Mostly just to count % of loops that we need to clear dma buffer

void setup() {
  Serial.begin(115200);
  timer.begin(heartbeat, 5000000); // Every 5 seconds
  packet_setup();
  analogReadResolution(16);
  SPI2.begin();
  SPI.begin(); // For dma
  imuSetup();
  dmaSetup();
  state = IDLE_STATE;

  delay(1000);
  runTests();
  //debugPrintf("Hey %d\n", assert(state != IDLE_STATE));
}

bool assertionError(const char* file, int line, const char* assertion) {
    errors++;
    bugs++;
    Serial.printf("%s, %d: assertion 'assertion' failed, total errors %d, bugs %d\n", file, line, errors, bugs);
    return false;
}

void heartbeat() {
    debugPrintf("State %d,", state);
    debugPrintf("transmitting %d, packetsReceived %d, ", transmitting, packetsReceived);
    debugPrintf("%d errors, bugs %d, last loop %d micros", errors, bugs, lastLoopTime);
    debugPrintf(", last state %d, last read %d", lastLoopState, lastAnalogRead);
    debugPrintf(", completed transfers %d, %d loops, time alive %d\n", completedDmaTransfers, numIdles, timeAlive);
    if (state == IMU_STATE) {
        imuHeartbeat();
    } else if (state == TRACKING_STATE) {
        trackingHeartbeat();
    } else if (state == CALIBRATION_STATE) {
        calibrationHeartbeat();
    }
}

void taskIdle(void) {
    // I can't put pins on SPI2 for now so just pray it works?
    uint16_t rando = random(60000);
    uint16_t receivedTransfer = SPI2.transfer16(rando);
    assert(receivedTransfer == rando);
    lastAnalogRead = analogRead(14);
    volatile int garbage = 0;
    for (int i = 0; i < 100000; i++) {
        garbage += i;
    }

    // TODO: error recovery on dma error

    if (!trx.busy()) {

        if (trx.done()) {
            compareBuffers(dmaSrc, dmaDest);
        }

        trx = DmaSpi::Transfer((const unsigned uint8_t *) dmaSrc, DMASIZE, dmaDest);
        clrDest((uint8_t*)dmaDest);
        DMASPI0.registerTransfer(trx);
        completedDmaTransfers++;
    }
    numIdles++;
}

void checkTasks(void) {
    long startOfLoop = micro;
    assert(state <= MAX_STATE);
    if (state == IDLE_STATE) {
        taskIdle();
    } else if (state == IMU_STATE) {
        taskIMU();
    } else if (state == TRACKING_STATE) {
        taskTracking();
    } else if (state == CALIBRATION_STATE) {
        taskCalibration();
    }

    // Save this into a long because elapsedMillis is not guaranteed in interrupts
    timeAlive = micro / 1000000;
    lastLoopTime = micro - startOfLoop;
    lastLoopState = state;
    // For now just assert, there isn't really a way to recover from a long main loop
    assert(lastLoopTime <= 1000000.0);
}

void loop() {
    checkTasks();
}
