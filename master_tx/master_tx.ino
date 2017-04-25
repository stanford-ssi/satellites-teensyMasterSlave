#include <SPI.h>
int i = 0;
int s;
int CHIPSELECT = 16;
SPISettings settingsA(62500, MSBFIRST, SPI_MODE0);
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(0, INPUT);
  s = millis();
  //SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.begin();
  pinMode(CHIPSELECT, OUTPUT);
  SPI.beginTransaction(settingsA);
}

void loop() {
  // put your main code here, to run repeatedly:
  //int x = digitalReadFast(0);

  digitalWrite(CHIPSELECT, LOW);
  delayMicroseconds(50);
  for (int j = 0; j < 256; j++) {
    //SPI.transfer(0);
    SPI.transfer((3*j)%256);
  
  }

  i++;
  if (i % 1000000 == 0) {
    //Serial.println(x);
    Serial.println(i);
    Serial.println(millis() - s);
  }
  delayMicroseconds(50);
  digitalWrite(CHIPSELECT, HIGH);
  
  //delay(1000);
}
