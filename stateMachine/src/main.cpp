#include "packet.h"
#include "main.h"
#include "states.h"

volatile uint16_t state = SETUP_STATE;

void setup() {
  Serial.begin(115200);
  packet_setup();
  analogReadResolution(16);
  SPI1.begin();
  state = IDLE_STATE;
}

void loop() {
    debugPrintf("State: %x\n", state);
    delay(5000);
}
