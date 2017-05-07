#include "packet.h"
#include "main.h"
#include "states.h"

T3SPI SPI_SLAVE;

// Incoming
volatile uint16_t packet[PACKET_SIZE + 10] = {}; // buffer just in case we overflow or something
volatile int packetPointer = 0;

// Outgoing
#define outBufferLength  300
volatile uint16_t outData[outBufferLength] = {};
volatile uint16_t *outBody = outData + OUT_PACKET_BODY_BEGIN;
volatile int outPointer = 0;
volatile uint16_t transmissionSize = 0;
volatile bool transmitting = false;

int k = 0;
volatile int bytesReceived = 0;
volatile int packetsReceived = 0;
volatile int timesCleared = 0;
volatile int timesBlocked = 0;
volatile int timesSent = 0;

void packet_setup(void) {
    SPI_SLAVE.begin_SLAVE(SCK, MOSI, MISO, T3_CS0);
    SPI_SLAVE.setCTAR_SLAVE(16, T3_SPI_MODE0);
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);

    pinMode(PACKET_RECEIVED_TRIGGER, OUTPUT);
    digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);

    // Not sure what priority this should be; this shouldn't fire at the same time as IRQ_SPI0
    attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
    attachInterrupt(PACKET_RECEIVED_PIN, handlePacket, FALLING);

    // Low priority for pin 26 -- packet received interrupt
    NVIC_SET_PRIORITY(IRQ_PORTE, 144);
}

//Interrupt Service Routine to handle incoming data
void spi0_isr(void) {
  //Function to handle data
  uint16_t to_send = EMPTY_WORD;
  if (transmitting && outPointer < transmissionSize) {
    to_send = outData[outPointer];
    outPointer++;
  }
  uint16_t received = SPI_SLAVE.rxtx16(to_send);
  Serial.printf("Received %x\n", received);
  if (packetPointer < PACKET_SIZE) {
    packet[packetPointer] = received;
    packetPointer++;
    if (packetPointer == PACKET_SIZE) {
      packetReceived();
    }
  }
}

void packetReceived() {
  noInterrupts();
  digitalWrite(PACKET_RECEIVED_TRIGGER, HIGH);
  digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);
  interrupts();
}

void handlePacket() {
  Serial.println("Received a packet!");
  if (transmitting || outPointer != 0) {
    responseBadPacket(INTERNAL_ERROR);
    Serial.println("I'm already transmitting!");
    return;
  }
  if (packetPointer != PACKET_SIZE) {
    responseBadPacket(INTERNAL_ERROR);
    Serial.printf("Recieved %d bytes, expected %d\n", packetPointer, PACKET_SIZE);
    return;
  }
  if (packet[0] != FIRST_WORD || packet[PACKET_SIZE - 1] != LAST_WORD) {
    responseBadPacket(INVALID_BORDER);
    Serial.printf("Invalid packet endings: start %x, end %x\n", packet[0], packet[PACKET_SIZE - 1]);
    return;
  }
  uint16_t receivedChecksum = 0;
  for (int i = PACKET_BODY_BEGIN; i < PACKET_BODY_END; i++) {
      receivedChecksum += packet[i];
  }
  if (receivedChecksum != packet[PACKET_SIZE - 2]) {
      responseBadPacket(INVALID_CHECKSUM);
      return;
  }

  response_echo();
}

void clearSendBuffer() {
    if (DEBUG) {
        for (int i = 0; i < outBufferLength; i++) {
            outData[i] = 0xbeef;
        }
    }
}

void response_echo() {
    clearSendBuffer();
    int bodySize = 7;
    for (int i = 0; i < bodySize; i++) {
      if (i < PACKET_SIZE) {
        outBody[i] = packet[i];
      } else {
        outBody[i] = 0;
      }
    }
    setupTransmission(RESPONSE_OK, bodySize);
}

void responseBadPacket(uint16_t flag) {
    clearSendBuffer();
    Serial.printf("Bad packet: flag %d\n", flag);
    int bodySize = 4;
    outBody[0] = flag;
    for (int i = 1; i < bodySize; i++) {
        outBody[i] = 0;
    }
    setupTransmission(RESPONSE_BAD_PACKET, bodySize);
}

// Call this after body of transmission is filled
void setupTransmission(uint16_t header, int bodyLength) {
    outPointer = 0;
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    uint16_t checksum = 0;
    outData[0] = FIRST_WORD;
    outData[1] = transmissionSize;
    outData[2] = header;
    for (int i = OUT_PACKET_BODY_BEGIN; i < transmissionSize - OUT_PACKET_BODY_END_SIZE; i++) {
      checksum += outData[i];
    }
    outData[transmissionSize - 2] = checksum;
    outData[transmissionSize - 1] = LAST_WORD;
    Serial.printf("Sending checksum: %x\n", checksum);
    transmitting = true;
}

void clearBuffer(void) {
  if (packetPointer != 0 && packetPointer != PACKET_SIZE) {
    Serial.printf("Clearing %d bytes of data\n", packetPointer);
  }
  packetPointer = 0;
  outPointer = 0;
  transmissionSize = 0;
  transmitting = false;
}

void clearBuffer2(void) {
  SPI_SLAVE.clearBuffer();
}
