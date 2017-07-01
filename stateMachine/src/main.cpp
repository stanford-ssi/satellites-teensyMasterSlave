#include "packet.h"
#include "main.h"
#include "states.h"

const volatile bool SPI2_LOOPBACK = false;
const volatile bool DMA_TRIGGER_LOOPBACK = false; // pin 17 to 18

IntervalTimer timer, timer2, timer3, timer4;
volatile uint16_t state = SETUP_STATE;
volatile uint16_t previousState = SETUP_STATE;
volatile bool changingState = false;
volatile unsigned int timeAlive = 0;
volatile int errors = 0;
volatile int bugs = 0;
long startOfLoop = 0;
volatile bool ignoreLoopTime = false;
volatile unsigned int lastLoopTime = 0;
volatile unsigned int maxLoopTime = 0;
volatile unsigned int lastLoopState = state;
volatile unsigned int lastAnalogRead = 0;
volatile unsigned int numLoops = 0; // Mostly just to count % of loops that we need to clear dma buffer


volatile unsigned int numFastLoops = 0;
volatile unsigned int numSlowLoops = 0;
volatile unsigned int numMediumLoops = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  debugPrintf("Serial is on\n");
  const int heartRate = 3000000;
  timer.begin(heartbeat, heartRate); // Every 3 seconds
  delay(1);
  timer2.begin(heartbeat2, heartRate);
  delay(1);
  timer3.begin(heartbeat3, heartRate);
  delay(1);
  timer4.begin(heartbeat4, heartRate);
  debugPrintf("Set up heartbeat interrupt\n");
  analogReadResolution(16);
  debugPrintf("Begin Spi2\n");
  SPI2.begin();
  debugPrintf("Setting up packet:\n");
  packet_setup();
  debugPrintf("Setting up imu state:\n");
  debugPrintf("Done.\n");
  pinMode(17, OUTPUT); // DMA loopback test
  digitalWriteFast(17, HIGH);
  debugPrintf("Setting up dma:\n");
  dmaSetup();
  debugPrintf("Done!\n");
  debugPrintf("Setting up tracking:\n");
  trackingSetup();
  debugPrintf("Done!\n");
  state = IDLE_STATE;
  delay(1000);
  debugPrintf("Running tests:\n");
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
    debugPrintf("%d errors, bugs %d, last loop %d micros, max loop time %d micros", errors, bugs, lastLoopTime, maxLoopTime);
    //ignoreLoopTime = true; // Reset this counter because we don't want to count heartbeat time in loop time
    maxLoopTime = 0;
}

void heartbeat2() {
    debugPrintf(", last state %d, last read %d, dma offset %d", lastLoopState, lastAnalogRead, dmaGetOffset());
    debugPrintf(", %d loops, time alive %d\n", numLoops, timeAlive);
    debugPrintf("%d last, %d max, %d fast, %d med, %d slow loops\n", lastLoopTime, maxLoopTime, numFastLoops, numMediumLoops, numSlowLoops);
}

void heartbeat3() {
    if (state == TRACKING_STATE) {
        trackingHeartbeat();
    } else if (state == CALIBRATION_STATE) {
        calibrationHeartbeat();
    }
    debugPrintf("-----------------------\n");
}

void heartbeat4() {}

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
    /*volatile int garbage = 0;
    for (int i = 0; i < 50000; i++) { // Do some work
        garbage += i;
    }*/
}

void leaveCurrentState() {
    assert(previousState != SHUTDOWN_STATE);
    if (previousState == IDLE_STATE) {
        //leaveIdle(); // Nothing to do here
    } else if (previousState == TRACKING_STATE) {
        leaveTracking();
    } else if (previousState == CALIBRATION_STATE) {
        leaveCalibration();
    }
}

void enterNextState() {
    if (state == IDLE_STATE) {
        //enterIdle(); // Nothing to do here
    } else if (state == TRACKING_STATE) {
        enterTracking();
    } else if (state == CALIBRATION_STATE) {
        enterCalibration();
    }
}

void checkTasks(void) {
    long startOfLoop = micros();
    assert(state <= MAX_STATE);
    if (changingState) {
        noInterrupts();
        leaveCurrentState();
        enterNextState();
        changingState = false;
        interrupts();
    }
    if (state == IDLE_STATE) {
        taskIdle();
    } else if (state == TRACKING_STATE) {
        taskTracking();
    } else if (state == CALIBRATION_STATE) {
        taskCalibration();
    }

    // Save this into a long because elapsedMillis is not guaranteed in interrupts
    timeAlive = millis();
    if (ignoreLoopTime) {
        ignoreLoopTime = false;
    } else {
        lastLoopTime = micros() - startOfLoop;
        lastLoopState = state;
        if (lastLoopTime > maxLoopTime) {
            maxLoopTime = lastLoopTime;
        }
        if (lastLoopTime < 10) {
            numFastLoops++;
        } else if (lastLoopTime < 100) {
            numMediumLoops++;
        } else {
            numSlowLoops++;
        }
        // For now just assert, there isn't really a way to recover from a long main loop
        assert(lastLoopTime <= 1000000.0);
    }
    numLoops++;
}

void loop() {
    checkTasks();
}
