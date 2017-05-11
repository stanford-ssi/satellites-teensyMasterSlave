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
volatile unsigned int lastAnalogRead = 0;

void setup() {
  Serial.begin(115200);
  packet_setup();
  analogReadResolution(16);
  SPI1.begin();
  state = IDLE_STATE;
  timer.begin(heartbeat, 10000000); // Every 10 seconds
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
    debugPrintf(", last read %d\n", lastAnalogRead);
}

void checkTasks(void) {
    long startOfLoop = micro;
    if (state == IDLE_STATE) {
        SPI1.transfer16(0xbeef);
        lastAnalogRead = analogRead(14);
    }

    // Save this into a long because elapsedMillis is not guaranteed in interrupts
    timeAlive = micro / 1000000;
    lastLoopTime = timeAlive - startOfLoop;
    // For now just assert, there isn't really a way to recover from a long main loop
    assert(lastLoopTime <= 1000.0);
}

void loop() {
    checkTasks();
}
