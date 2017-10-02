// Used wiringPiSpi library demo -- see below.
// Demo code taken from https://github.com/sparkfun/Pi_Wedge -- thanks to Byron Jacquot @ SparkFun Electronics
// WiringPiSpi library here: http://wiringpi.com/download-and-install/

#include <iostream>
#include <fstream>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace std;

#define LINES_TO_LOG 1000

unsigned int errors = 0;
unsigned int bugs = 0;
const int spiSpeed = 6250000;

ofstream fout;
int linesLogged = 0;

bool assertionError(const char* file, int line, const char* assertion) {
    errors++;
    bugs++;
    printf("%s, %d: assertion 'assertion' failed, total errors %u, bugs %u\n", file, line, errors, bugs);
    //sleep(1);
    return false;
}
#define DEBUG true
#define assert(e) (((e) || !DEBUG) ? (true) : (assertionError(__FILE__, __LINE__, #e), false));

// channel is the wiringPi name for the chip select (or chip enable) pin.
// Set this to 0 or 1, depending on how it's connected.
static const int CHANNEL = 0;

#define PACKET_BODY_LENGTH 7
bool transmitMany = false;
int bytesSent = 0;
const int KILLSWITCH = 8; // wiringPi 8, pin 3 sda1
const int CHIPSELECT = 25; // wiringPi 15, pin 8 txd0
const int REVERSECS = 24; // wiringPi 16, pin 10 rxd0
int incomingByte = 0;
int numError = 0;

#define RESPONSE_OK 0
#define RESPONSE_BAD_PACKET 1
#define RESPONSE_IMU_DATA 2
#define RESPONSE_MIRROR_DATA 3
#define RESPONSE_ADCS_REQUEST 4
#define RESPONSE_PROBE 5

uint16_t echo[PACKET_BODY_LENGTH] = {0x0, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t stat[PACKET_BODY_LENGTH] = {0x1, 0xaaaa, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t idle_[PACKET_BODY_LENGTH] = {0x2, 0xcccc, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t shutdown_[PACKET_BODY_LENGTH] = {0x3, 0xdddd, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t enterCalibration[PACKET_BODY_LENGTH] = {0x6, 1, 7, 500, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t enterPointTrack[PACKET_BODY_LENGTH] = {0x7, 0xffff, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t reportTrack[PACKET_BODY_LENGTH] = {0x8, 0xffff, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t probe[PACKET_BODY_LENGTH] = {0x9, 32, 0x1fff, 0x0848, 0xbbbb, 0xbbbb, 0xbbbb};
uint16_t writeMem[PACKET_BODY_LENGTH] = {10, 32, 0x1fff, 0x0848, 0x1, 0x2, 0x3};
uint16_t set_constant_uint[PACKET_BODY_LENGTH] = {11, 1, 0, 0, 0, 0, 0};
uint16_t set_constant_float[PACKET_BODY_LENGTH] = {11, 51, 1, 65535, 64304, 0, 0}; // -1234

void rando();
void loop();

int main()
{
    fout.open("log.csv");
    wiringPiSetup () ;
    wiringPiSPISetup(CHANNEL, spiSpeed);
    pinMode(REVERSECS, INPUT);
    pinMode(CHIPSELECT, OUTPUT);
    pinMode(KILLSWITCH, OUTPUT);
    digitalWrite(KILLSWITCH, HIGH);
    digitalWrite(CHIPSELECT, HIGH);
    cout << "Starting" << endl;
    //unsigned char buf[2];
    while (1) {
        /*buf[0] = 0x12;
        buf[1] = 0xab;
       wiringPiSPIDataRW(CHANNEL, (unsigned char *) buf, 2 );*/
       loop();
   }
}

void transmitH(uint16_t *buf, bool verbos);

void transmit(uint16_t *buf) {
  if (transmitMany) {
    transmitMany = false;
    for (int i = 0; i < 30000; i++) {
      transmitH(buf, false);
    }
    printf("Done\n");
  } else {
    transmitH(buf, true);
  }
}

void setBuf(uint16_t* to_send, int& j, uint16_t val) {
    unsigned char * bytes = (unsigned char *) to_send;
    bytes[2 * j] = (unsigned char) (val >> 8);
    bytes[2 * j + 1] = (unsigned char) (val % (1 << 8));
    j++;
}

uint16_t getBuf(uint16_t* buf, int j) {
    uint8_t * bytes = (uint8_t *) buf;
    uint16_t val = ((uint16_t) bytes[2 * j]) << 8;
    val += bytes[2 * j + 1];
    return val;
}

void transmitH(uint16_t *buf, bool verbos) {
    digitalWrite(CHIPSELECT, HIGH);
    digitalWrite(CHIPSELECT, LOW);
    //delayMicroseconds(1);
    int j = 0;
    uint16_t to_send[10000];
    setBuf(to_send, j, 0x1234);
    uint16_t checksum = 0;
    for (int i = 0; i < PACKET_BODY_LENGTH; i++) {
        setBuf(to_send, j, buf[i]);

        checksum += buf[i];
    }
    setBuf(to_send, j, checksum);
    setBuf(to_send, j, 0x4321);

    int numIters = 550;

    memset(to_send + j, 0xff, 2 * numIters);
    uint16_t to_send_copy[10000];
    memcpy(to_send_copy, to_send, 2 * numIters + 100);
    wiringPiSPIDataRW(CHANNEL, (unsigned char *) to_send, 2 * (numIters));

    int i;

    // Track to beginning of response
    for (i = 0; i < numIters; i++) {
        if (getBuf(to_send, i) == 0x1234) {
            break;
        }
    }
    if (i == numIters) {
        cout << "No response" << endl;
    }
    uint16_t len = getBuf(to_send, i+1);
    uint16_t lenCheck = ~getBuf(to_send, i+2);
    assert(len == lenCheck);
    assert(len <= 600);
    if (len >= 600 || len != lenCheck) {
        cout << "Truncated" << endl;
        len = 75;
    }
    uint16_t computedChecksum = 0;
    uint16_t checksumIndex = len + i - 2;
    for (int j = i+1; j < checksumIndex; j++) {
        computedChecksum += getBuf(to_send, j);
    }
    assert(computedChecksum == getBuf(to_send, checksumIndex));
    uint16_t responseNumber = getBuf(to_send, i+3);
    if (responseNumber == RESPONSE_MIRROR_DATA && linesLogged < LINES_TO_LOG) {
        linesLogged += 1;
        for (int j = checksumIndex - (15 * 24); j < checksumIndex; j+=24) {
            for (int k = 0; k < 22; k+=2) {
                fout << (int) (((unsigned int) getBuf(to_send, j + k)) * (1 << 16) + (unsigned int) getBuf(to_send, j + k + 1)) << ",";
            }
            fout << (int) (getBuf(to_send, j + 22) * (1 << 16) + getBuf(to_send, j + 22 + 1)) << "," << errors << "," << computedChecksum << "," << getBuf(to_send, checksumIndex) << endl;
        }
    }
    uint16_t numToPrint = len + i + 2;
    assert(getBuf(to_send, len + i - 1) == 0x4321);
    if (numToPrint > 600) {
        numToPrint = 75;
    }
    for (i = 0; i < numToPrint; i++) {
        if (verbos) {
            printf("i %d\tSent: %x, received %x\n", i, getBuf(to_send_copy, i), getBuf(to_send, i));
        }
    }

    if (verbos) {
        printf("Response header %d ", responseNumber);
        if (responseNumber == RESPONSE_OK) {
            printf("Ok");
        } else if (responseNumber == RESPONSE_BAD_PACKET) {
            printf("Bad packet");
        } else if (responseNumber == RESPONSE_MIRROR_DATA) {
            printf("Mirror data");
        } else if (responseNumber == RESPONSE_ADCS_REQUEST) {
            printf("ADCS request");
        } else if (responseNumber == RESPONSE_IMU_DATA) {
            printf("IMU data");
        } else if (responseNumber == RESPONSE_PROBE) {
            printf("Probe");
        } else {
            printf("Bad header");
        }
        cout << endl;
        cout << "Num: " << numToPrint << endl;
        cout << "Errors: " << errors << endl;
        printf("---------------------------------\n");
    }
    delayMicroseconds(1);
    digitalWrite(CHIPSELECT, HIGH);
}

void transmitCrappy(uint16_t *buf) {
    /*
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
  digitalWrite(CHIPSELECT, HIGH);*/
}

void rando() {
  printf("begin random\n");

  int i = 0;
  while(true) {
    int nextCommand = rand() % 3;
    if (nextCommand == 0) {
      transmitH(echo, false);
    } else if (nextCommand == 1) {
      transmitH(stat, false);
    } else if (nextCommand == 2) {
      transmitH(idle_, false);
    } else if (nextCommand == 3) {
      transmitH(enterPointTrack, false);
    }
    i++;
  }
  printf("end random, send %d packets\n", i);
}

void loop() {
    // send data only when you receive data:
    char incomingByte;
    cin >> incomingByte;
    if (incomingByte == '\n') {
    } else if (incomingByte == '1') {
      transmit(echo);
    } else if (incomingByte == '2') {
      transmit(stat);
    } else if (incomingByte == '3') {
        transmit(idle_);
    } else if (incomingByte == '6') {
      transmit(enterPointTrack);
    } else if (incomingByte == '7') {
      transmit(reportTrack);
    } else if (incomingByte == 'p') {
      while (true) {
      //for (int i = 0; i < 100000000000; i++) {
          for (volatile int i = 0; digitalRead(24) == 0; i++) {
          }
          transmitH(reportTrack, false);
      }
    } else if (incomingByte == 's') {
      transmit(shutdown_);
    } else if (incomingByte == '8') {
      transmit(enterCalibration);
    } else if (incomingByte == '9') {
      transmit(probe);
    } else if (incomingByte == 'w') {
      transmit(writeMem);
    } else if (incomingByte == 'u') {
      transmit(set_constant_uint);
    } else if (incomingByte == 'f') {
      transmit(set_constant_float);
    } else if (incomingByte == 'c') {
      transmitCrappy(echo);
    } else if (incomingByte == 'd') {
      cout << digitalRead(24) << endl;
    } else if (incomingByte == 'r') {
      rando();
    } else if (incomingByte == 'm') {
      transmitMany = true;
    } else if (incomingByte == 'l') {
        printf("1 - echo\n2 - stat\n3 - idle\n4 - imu\n5 - imuData\n6 - point\n7 - report tracking\n8 - cal\n9 - probe\nc - crappy\np - report\nr - rando\n");
    }
}
