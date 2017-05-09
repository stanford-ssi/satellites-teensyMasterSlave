#include <SPI.h>
#define PACKET_BODY_LENGTH 2
int bytesSent = 0;
int CHIPSELECT = 16;
int incomingByte = 0;
uint16_t echo[PACKET_BODY_LENGTH] = {0xbbbb, 0xcccc};

SPISettings settingsA(6250000, MSBFIRST, SPI_MODE1);
void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT);
  SPI.begin();
  pinMode(CHIPSELECT, OUTPUT);
  SPI.beginTransaction(settingsA);
  delay(1000);
  digitalWrite(CHIPSELECT, HIGH);
}

void send16(uint16_t to_send) {
  Serial.printf("Sent: %x, received: %x\n", to_send, SPI.transfer16(to_send));;
}

void transmit(uint16_t *buf) {
  digitalWrite(CHIPSELECT, HIGH);
  digitalWrite(CHIPSELECT, LOW);
  delay(1);
  send16(0x1234);
  uint16_t checksum = 0;
  for (int i = 0; i < PACKET_BODY_LENGTH; i++) {
    send16(buf[i]);
    checksum += buf[i];
  }
  send16(checksum);
  send16(0x4321);
  for (int i = 0; i < 30; i++) {
    send16(0xffff);
  }
  Serial.println("---------------------------------");
  delay(1);
  digitalWrite(CHIPSELECT, HIGH);
}

void transmitCrappy(uint16_t *buf) {
  digitalWrite(CHIPSELECT, HIGH);
  digitalWrite(CHIPSELECT, LOW);
  delay(1);
  send16(0x1233);
  uint16_t checksum = 0;
  for (int i = 0; i < PACKET_BODY_LENGTH; i++) {
    send16(buf[i]);
    checksum += buf[i];
  }
  send16(checksum);
  send16(0x4321);
  for (int i = 0; i < 30; i++) {
    send16(0xffff);
  }
  Serial.println("---------------------------------");
  delay(1);
  digitalWrite(CHIPSELECT, HIGH);
}

void loop() {
  // send data only when you receive data:
  if (Serial.available() > 0) {
      // read the incoming byte:
      incomingByte = Serial.read();
      if (incomingByte == '\n') {
      } else if (incomingByte == '1') {
        transmit(echo);
      } else if (incomingByte == '2') {
        transmitCrappy(echo);
      }
  }
}
