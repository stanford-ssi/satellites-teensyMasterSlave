#include "packet.h"
#include "main.h"
#include "states.h"

T3SPI SPI_SLAVE;

// Very little effort is made to prevent these from overflowing
// They will only be used for basic telemetry
volatile unsigned int packetsReceived = 0;
volatile unsigned int wordsReceived = 0;

// Incoming
volatile uint16_t packet[PACKET_SIZE + 10] = {}; // buffer just in case we overflow or something
volatile int packetPointer = 0;

// Outgoing
#define outBufferLength  300
volatile uint16_t outData[outBufferLength] = {};
volatile uint16_t *outBody = outData + OUT_PACKET_BODY_BEGIN;
volatile unsigned int outPointer = 0;
volatile uint16_t transmissionSize = 0;
volatile bool transmitting = false;

void packet_setup(void) {
    assert(outPointer == 0);
    assert(transmissionSize == 0);
    assert(transmitting == false);
    SPI_SLAVE.begin_SLAVE(SCK1, MOSI1, MISO1, T3_SPI1_CS0);
    SPI_SLAVE.setCTAR_SLAVE(16, T3_SPI_MODE0);
    NVIC_ENABLE_IRQ(IRQ_SPI1);
    NVIC_SET_PRIORITY(IRQ_SPI1, 16);

    pinMode(PACKET_RECEIVED_TRIGGER, OUTPUT);
    digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);

    // Not sure what priority this should be; this shouldn't fire at the same time as IRQ_SPI0
    attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
    attachInterrupt(PACKET_RECEIVED_PIN, handlePacket, FALLING);

    // Low priority for pin 26 -- packet received interrupt
    NVIC_SET_PRIORITY(IRQ_PORTE, 144);
}

//Interrupt Service Routine to handle incoming data
void spi1_isr(void) {
  assert(outPointer <= transmissionSize);
  assert(packetPointer <= PACKET_SIZE);
  assert(transmitting || outPointer == 0);
  wordsReceived++;
  uint16_t to_send = EMPTY_WORD;
  if (transmitting && outPointer < transmissionSize) {
    to_send = outData[outPointer];
    outPointer++;
  }
  uint16_t received = SPI_SLAVE.rxtx16(to_send);
  if (packetPointer < PACKET_SIZE) {
    //debugPrintf("Received %x\n", received);
    packet[packetPointer] = received;
    packetPointer++;
    if (packetPointer == PACKET_SIZE) {
      packetReceived();
    }
  }
}

void packetReceived() {
  assert(packetPointer == PACKET_SIZE);
  packetsReceived++;

  noInterrupts();
  digitalWrite(PACKET_RECEIVED_TRIGGER, HIGH);
  digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);
  interrupts();
}

void handlePacket() {
  //debugPrintln("Received a packet!");
  assert(packetPointer == PACKET_SIZE);

  // Check for erroneous data
  if (transmitting || outPointer != 0) {
    debugPrintln("I'm already transmitting!");
    // No response because we're presumably already transmitting
    // responseBadPacket(INTERNAL_ERROR);
    return;
  }
  if (packetPointer != PACKET_SIZE) {
    debugPrintf("Received %d bytes, expected %d\n", packetPointer, PACKET_SIZE);
    responseBadPacket(INTERNAL_ERROR);
    return;
  }
  if (packet[0] != FIRST_WORD || packet[PACKET_SIZE - 1] != LAST_WORD) {
    debugPrintf("Invalid packet endings: start %x, end %x\n", packet[0], packet[PACKET_SIZE - 1]);
    responseBadPacket(INVALID_BORDER);
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

  create_response();
}

void create_response() {
    assert(packetPointer == PACKET_SIZE);
    assert(transmitting == false);
    uint16_t command = packet[1];
    if (command == COMMAND_ECHO) {
        response_echo();
    } else if (command == COMMAND_STATUS) {
        response_status();
    } else if (command == COMMAND_SHUTDOWN) {
        state = SHUTDOWN_STATE;
        response_status();
    } else if (command == COMMAND_IDLE) {
        state = IDLE_STATE;
        response_status();
    } else {
        responseBadPacket(INVALID_COMMAND);
    }
}

void clearSendBuffer() {
    if (DEBUG) {
        for (int i = 0; i < outBufferLength; i++) {
            outData[i] = 0xbeef;
        }
    }
}

void response_echo() {
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    int bodySize = 7;
    for (int i = 0; i < bodySize; i++) {
      if (i < PACKET_SIZE) {
        outBody[i] = packet[i];
      } else {
        outBody[i] = 0;
      }
    }
    assert(outBody[bodySize] == 0xbeef);
    setupTransmission(RESPONSE_OK, bodySize);
}

void response_status() {
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    int bodySize = 9;
    outBody[0] = state;
    outBody[1] = packetsReceived / (1 << 16);
    outBody[2] = packetsReceived % (1 << 16);
    outBody[3] = wordsReceived / (1 << 16);
    outBody[4] = wordsReceived % (1 << 16);
    volatile unsigned int timeAliveSave = timeAlive;
    outBody[5] = timeAliveSave / (1 << 16);
    outBody[6] = timeAliveSave % (1 << 16);
    volatile unsigned int lastLoopSave = lastLoopTime;
    outBody[7] = lastLoopSave / (1 << 16);
    outBody[8] = lastLoopSave % (1 << 16);
    assert(outBody[bodySize] == 0xbeef);
    setupTransmission(RESPONSE_OK, bodySize);
}

void responseBadPacket(uint16_t flag) {
    errors++;
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
    debugPrintf("Bad packet: flag %d\n", flag);
    unsigned int bodySize = 4;
    outBody[0] = flag;
    for (unsigned int i = 1; i < bodySize; i++) {
        outBody[i] = 0;
    }
    assert(outBody[bodySize] == 0xbeef);
    setupTransmission(RESPONSE_BAD_PACKET, bodySize);
}

// Call this after body of transmission is filled
void setupTransmission(uint16_t header, unsigned int bodyLength) {
    assert(header <= MAX_HEADER);
    assert(bodyLength <= outBufferLength - OUT_PACKET_OVERHEAD);
    assert(bodyLength > 0);
    outPointer = 0;
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    outData[0] = FIRST_WORD;
    outData[1] = transmissionSize;
    outData[2] = header;
    assert(outBody[0] != 0xbeef);
    assert(outBody[bodyLength - 1] != 0xbeef);
    uint16_t checksum = transmissionSize + header;
    for (int i = OUT_PACKET_BODY_BEGIN; i < transmissionSize - OUT_PACKET_BODY_END_SIZE; i++) {
      checksum += outData[i];
    }
    outData[transmissionSize - 2] = checksum;
    outData[transmissionSize - 1] = LAST_WORD;
    //debugPrintf("Sending checksum: %x\n", checksum);
    transmitting = true;
}

void clearBuffer(void) {
  if (packetPointer != 0 && packetPointer != PACKET_SIZE) {
    debugPrintf("Clearing %d bytes of data\n", packetPointer);
  }
  noInterrupts();
  packetPointer = 0;
  outPointer = 0;
  transmissionSize = 0;
  transmitting = false;
  interrupts();
}
