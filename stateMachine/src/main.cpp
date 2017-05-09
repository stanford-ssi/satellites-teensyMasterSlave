#include "packet.h"
#include "main.h"
#include "states.h"

IntervalTimer timer;
volatile uint16_t state = SETUP_STATE;

void setup() {
  Serial.begin(115200);
  packet_setup();
  analogReadResolution(16);
  SPI1.begin();
  state = IDLE_STATE;
  timer.begin(heartbeat, 3000000);
}

void heartbeat() {
    debugPrintf("Received %d words, sent %d, transmissionSize %d, transmitting %d, packetsReceived %d\n", packetPointer, outPointer, transmissionSize, transmitting, packetsReceived);
}

void loop() {
    debugPrintf("State: %x\n", state);
    for (int i = 0; i < 400000; i++) {
        SPI1.transfer16(0xbeef);
    }
    delay(5000);
}
