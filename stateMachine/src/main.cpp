#include "packet.h"
#include "main.h"
#include "states.h"

const volatile bool SPI2_LOOPBACK = false;
const volatile bool DMA_TRIGGER_LOOPBACK = false; // pin 17 to 18

IntervalTimer timer;
volatile uint16_t state = SETUP_STATE;
elapsedMicros micro = 0;
volatile unsigned int timeAlive = 0;
volatile int errors = 0;
volatile int bugs = 0;
volatile unsigned int lastLoopTime = 0;
volatile unsigned int lastLoopState = state;
volatile unsigned int lastAnalogRead = 0;
volatile unsigned int numIdles = 0; // Mostly just to count % of loops that we need to clear dma buffer

void setup() {
  Serial.begin(115200);
  timer.begin(heartbeat, 3000000); // Every 3 seconds
  analogReadResolution(16);
  SPI2.begin();
  packet_setup();
  imuSetup();
  pinMode(17, OUTPUT); // DMA loopback test
  digitalWriteFast(17, HIGH);
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
    debugPrintf(", last state %d, last read %d, dma offset %d", lastLoopState, lastAnalogRead, dmaGetOffset());
    debugPrintf(", %d loops, time alive %d\n", numIdles, timeAlive);
    if (state == IMU_STATE) {
        imuHeartbeat();
    } else if (state == TRACKING_STATE) {
        trackingHeartbeat();
    } else if (state == CALIBRATION_STATE) {
        calibrationHeartbeat();
    }
}

void taskIdle(void) {
    uint16_t rando = random(60000);
    uint16_t receivedTransfer = SPI2.transfer16(rando);
    assert(!SPI2_LOOPBACK || receivedTransfer == rando);
    lastAnalogRead = analogRead(14);
    if (DMA_TRIGGER_LOOPBACK) {
        assert(!dmaSampleReady());
        for (int i = 0; i < DMA_SAMPLE_NUMAXES; i++) {
            digitalWriteFast(17, LOW);
            delayMicroseconds(100);
            digitalWriteFast(17, HIGH);
            delayMicroseconds(1000);
        }
        assert(dmaSampleReady());
        dmaGetSample();
        assert(!dmaSampleReady());
    }
    volatile int garbage = 0;
    for (int i = 0; i < 100000; i++) { // Do some work
        garbage += i;
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
