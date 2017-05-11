#include <SPI.h>

IntervalTimer myTimer;
elapsedMillis milli;

volatile int count;

void setup(void) {
  Serial.begin(9600);
  SPI.begin();
  myTimer.begin(counter, 10);  // blinkLED to run every 0.15 seconds
  milli = 0;
}



void loop() {
  SPI.transfer(0xff);
  if (count % 100 == 0) {
    Serial.printf("count %d, per sec %d\n", count, count / (milli / 1000));
  }
}

void counter(void) {
  count++;
}
