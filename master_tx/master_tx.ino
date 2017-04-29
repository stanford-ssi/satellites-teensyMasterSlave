#include <SPI.h>
int bytesSent = 0;
int s;
int CHIPSELECT = 16;
uint8_t checksum = 0;
SPISettings settingsA(6250000, MSBFIRST, SPI_MODE0);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(0, INPUT);
  s = millis();
  //SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.begin();
  pinMode(CHIPSELECT, OUTPUT);
  SPI.beginTransaction(settingsA);
  delay(1000);

  checksum = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  
  digitalWrite(CHIPSELECT, LOW);
  delayMicroseconds(20);
  for (int j = 0; j < 255; j++) {
    //SPI.transfer(0);
    uint8_t toSend = (bytesSent * 17)%255; // Arbitrary data
    checksum += toSend;
    SPI.transfer(toSend);
    bytesSent++;
  }

  SPI.transfer(checksum);
  bytesSent++;
  
  if (bytesSent % 10000000 == 0) {
    //Serial.println(x);
    Serial.println(bytesSent);
    Serial.println(millis() - s);
  }
  delayMicroseconds(20);
  digitalWrite(CHIPSELECT, HIGH);
  
  //delay(100000);
}
