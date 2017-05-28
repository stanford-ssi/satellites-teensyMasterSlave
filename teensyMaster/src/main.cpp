#include <SPI.h>
#define PACKET_BODY_LENGTH 2
bool transmitMany = false;
int bytesSent = 0;
int CHIPSELECT = 16;
int incomingByte = 0;
int numError = 0;
int numIter = 0;
uint16_t echo[PACKET_BODY_LENGTH] = {0x0, 0xbbbb};
uint16_t stat[PACKET_BODY_LENGTH] = {0x1, 0xaaaa};
uint16_t idle_[PACKET_BODY_LENGTH] = {0x2, 0xcccc};
uint16_t shutdown_[PACKET_BODY_LENGTH] = {0x3, 0xdddd};
uint16_t enterImu[PACKET_BODY_LENGTH] = {0x4, 0xcccc};
uint16_t getImuData[PACKET_BODY_LENGTH] = {0x5, 0xeeee};

void rando();

SPISettings settingsA(6250000, MSBFIRST, SPI_MODE0);
void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT);
  SPI.begin();
  pinMode(CHIPSELECT, OUTPUT);
  SPI.beginTransaction(settingsA);
  delay(1000);
  digitalWrite(CHIPSELECT, HIGH);
  //rando();
  Serial.println("hey\n");
}

void transmitH(uint16_t *buf, bool verbos);

uint16_t send16(uint16_t to_send, bool verbos) {
  uint16_t dat = SPI.transfer16(to_send);
  if (verbos) {
    Serial.printf("Sent: %x, received: %x\n", to_send, dat);
  }
  return dat;
}

void transmit(uint16_t *buf) {
  if (transmitMany) {
    transmitMany = false;
    for (int i = 0; i < 30000; i++) {
      transmitH(buf, false);
    }
    Serial.println("Done");
  } else {
    transmitH(buf, true);
  }
}

void transmitH(uint16_t *buf, bool verbos) {
  digitalWrite(CHIPSELECT, HIGH);
  digitalWrite(CHIPSELECT, LOW);
  delayMicroseconds(80);
  send16(0x1234, verbos);
  uint16_t checksum = 0;
  for (int i = 0; i < PACKET_BODY_LENGTH; i++) {
    send16(buf[i], verbos);
    checksum += buf[i];
  }
  send16(checksum, verbos);
  send16(0x4321, verbos);
  int numIters = 30;
  if (buf == getImuData) {
    numIters = 310;
  }
  uint16_t received;
  for (int i = 0; i < numIters; i++) {
    received = send16(0xffff, verbos);
    if (received == 0x1234) {
      break;
    }
  }
  if (received != 0x1234) {
    Serial.println("bad");
    numError++;
  }
  uint16_t len = send16(0xffff, verbos);
  uint16_t reponseNumber = send16(0xffff, verbos);
  for (int i = 0; i < len - 2; i++) {
    send16(0xffff, verbos);
  }
  if(verbos) {
    Serial.println("---------------------------------");
  }
  delayMicroseconds(80);
  digitalWrite(CHIPSELECT, HIGH);
}

void transmitCrappy(uint16_t *buf) {
  digitalWrite(CHIPSELECT, HIGH);
  digitalWrite(CHIPSELECT, LOW);
  delay(1);
  send16(0x1233, true);
  uint16_t checksum = 0;
  for (int i = 0; i < PACKET_BODY_LENGTH; i++) {
    send16(buf[i], true);
    checksum += buf[i];
  }
  send16(checksum, true);
  send16(0x4321, true);
  for (int i = 0; i < 30; i++) {
    send16(0xffff, true);
  }
  Serial.println("---------------------------------");
  delay(1);
  digitalWrite(CHIPSELECT, HIGH);
}

void rando() {
  Serial.println("begin random");
  int i = 0;
  while (Serial.available() <= 0) {
    int nextCommand = random(3);
    if (nextCommand == 0) {
      transmitH(echo, false);
    } else if (nextCommand == 1) {
      transmitH(stat, false);
    } else if (nextCommand == 2) {
      transmitH(idle_, false);
    } else {
      return;
    }/*else if (nextCommand == 3) {
    }
      transmitH(shutdown_, false);
    }*/
    i++;
  }
  Serial.printf("end random, send %d packets\n", i);
}

void loop() {
  Serial.printf("hey%d\n", numIter);
  numIter++;
  // send data only when you receive data:
  if (Serial.available() > 0) {
      // read the incoming byte:
      incomingByte = Serial.read();
      if (incomingByte == '\n') {
      } else if (incomingByte == '1') {
        transmit(echo);
      } else if (incomingByte == '2') {
        transmit(stat);
      } else if (incomingByte == '3') {
        transmit(idle_);
      } else if (incomingByte == '4') {
        transmit(enterImu);
      } else if (incomingByte == '5') {
        transmit(getImuData);
      } else if (incomingByte == '6') {
        transmitCrappy(echo);
      } else if (incomingByte == 'r') {
        while (Serial.available() > 0) {
          Serial.read();
        }
        rando();
      } else if (incomingByte == 'm') {
        transmitMany = true;
      }
  }
}
