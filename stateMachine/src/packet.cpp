#include "packet.h"
#include "main.h"
#include "states.h"
#include "pidData.h"
#include <DMAChannel.h>

T3SPI SPI_SLAVE;

// Very little effort is made to prevent these from overflowing
// They will only be used for basic telemetry
volatile unsigned int packetsReceived = 0;
volatile unsigned int wordsReceived = 0;

DMAChannel dma_rx;
DMAChannel dma_tx;
uint32_t beef_only[11];
const uint16_t buffer_size = 600;
uint16_t packet[buffer_size];
uint32_t spi_tx_out[buffer_size];
volatile bool transmitting = false;

// Outgoing
volatile uint32_t *outData = spi_tx_out + ABCD_BUFFER_SIZE;
volatile uint32_t *outBody = outData + OUT_PACKET_BODY_BEGIN;
volatile uint32_t *currentlyTransmittingPacket = outData; // One doesn't always have to supply outData as the packet buffer
volatile uint16_t transmissionSize = 0;

bool shouldClearSendBuffer = false;

// Local functions
void response_echo();
void response_status();
void responseBadPacket(uint16_t flag);
void create_response();
void responseImuDump();
void setupTransmission(uint16_t header, unsigned int bodyLength);
void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint32_t *packetBuffer);

void received_packet_isr(void)
{
    noInterrupts();
    assert(packet[0] == 0x1234);
    dma_rx.disable();
    dma_tx.disable();
    dma_rx.clearInterrupt();
    if (!transmitting) {
        packetReceived();
        dma_rx.destinationBuffer((uint16_t*) packet + PACKET_SIZE, 2 * 500);
        dma_tx.sourceBuffer((uint32_t *) currentlyTransmittingPacket - ABCD_BUFFER_SIZE, 2 * 500);
        dma_tx.triggerAtTransfersOf(dma_rx);
        transmitting = true;
        dma_tx.enable();
        dma_rx.enable();
    }
    debugPrintf("Hey\n");
    interrupts();
}

void setup_dma_receive(void) {
    for (int i = 0; i < 500; i++) {
        spi_tx_out[i] = 0xffff0100 + i;
    }
    for (int i = 0; i < 11; i++) {
        beef_only[i] = 0xabcd;
    }
    for (int i = 0; i < ABCD_BUFFER_SIZE; i++) {
        spi_tx_out[i] = 0xabcd;
    }
    dma_rx.source((uint16_t&) KINETISK_SPI1.POPR);
    dma_rx.destinationBuffer((uint16_t*) packet, PACKET_SIZE * 2);
    dma_rx.disableOnCompletion();
    dma_rx.interruptAtCompletion();
    dma_rx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI1_RX);
    dma_rx.attachInterrupt(received_packet_isr);

    dma_tx.sourceBuffer((uint32_t *) beef_only, PACKET_SIZE * 2);
    dma_tx.destination(KINETISK_SPI1.PUSHR); // SPI1_PUSHR_SLAVE
    dma_tx.disableOnCompletion();
    dma_tx.triggerAtTransfersOf(dma_rx);

    SPI1_RSER = SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS; // DMA on receive FIFO drain flag
    SPI1_SR = 0xFF0F0000;

    dma_rx.enable();
    dma_tx.enable();
}

void packet_setup(void) {
    assert(transmissionSize == 0);
    assert(transmitting == false);
    SPI_SLAVE.begin_SLAVE(SCK1, MOSI1, MISO1, T3_SPI1_CS0);
    SPI_SLAVE.setCTAR_SLAVE(16, T3_SPI_MODE0);
    setup_dma_receive();

    attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
}

uint16_t getHeader() {
    return currentlyTransmittingPacket[3];
}

//Interrupt Service Routine to handle incoming data
/*void spi1_isr(void) {
  assert(outPointer <= transmissionSize);
  assert(packetPointer <= PACKET_SIZE);
  assert(transmitting || outPointer == 0);
  wordsReceived++;
  uint16_t to_send = EMPTY_WORD;
  if (transmitting && outPointer < transmissionSize) {
    to_send = currentlyTransmittingPacket[outPointer];
    outPointer++;
    if (outPointer == transmissionSize && ((state == TRACKING_STATE) || (state == CALIBRATION_STATE)) && getHeader() == RESPONSE_PID_DATA) {
        assert(imuPacketReady);
        imuPacketSent();
    }
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
}*/

void packetReceived() {
  packetsReceived++;

  handlePacket();
}

void handlePacket() {
  unsigned int startTimePacketPrepare = micros();

  // Check for erroneous data
  if (transmitting) {
    debugPrintln("Error: I'm already transmitting!");
    // No response because we're presumably already transmitting
    // responseBadPacket(INTERNAL_ERROR);
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

  unsigned int midTimePacketPrepare = micros();
  (void) midTimePacketPrepare;

  create_response();

  unsigned int endTimePacketPrepare = micros();
  //assert (endTimePacketPrepare - startTimePacketPrepare <= 1);
  if (endTimePacketPrepare - startTimePacketPrepare > 1) {
      debugPrintf("Packet took %d, %d micros\n", endTimePacketPrepare - startTimePacketPrepare, midTimePacketPrepare - startTimePacketPrepare);
  }
}

void create_response() {
    //assert(packetPointer == PACKET_SIZE);
    assert(transmitting == false);
    uint16_t command = packet[1];
    if (changingState) {
        responseBadPacket(STATE_NOT_READY);
        return;
    }
    if (command == COMMAND_ECHO) {
        response_echo();
    } else if (command == COMMAND_STATUS) {
        response_status();
    } else if (command == COMMAND_SHUTDOWN) {
        previousState = state;
        changingState = true;
        state = SHUTDOWN_STATE;
        response_status();
    } else if (command == COMMAND_IDLE) {
        previousState = state;
        changingState = true;
        state = IDLE_STATE;
        response_status();
    } else if (command == COMMAND_POINT_TRACK) {
        previousState = state;
        changingState = true;
        state = TRACKING_STATE;
        response_status();
    } else if (command == COMMAND_CALIBRATE) {
        previousState = state;
        changingState = true;
        state = CALIBRATION_STATE;
        response_status();
    } else if (command == COMMAND_REPORT_TRACKING) {
        if (!(state == TRACKING_STATE || state == CALIBRATION_STATE)) {
            responseBadPacket(INVALID_COMMAND);
        } else if (!imuPacketReady) {
            debugPrintf("Front of buf %d back %d samples sent %d\n", imuDataPointer, imuSentDataPointer, imuSamplesSent);
            responseBadPacket(DATA_NOT_READY);
        } else {
            responseImuDump();
        }
    } else {
        responseBadPacket(INVALID_COMMAND);
    }
}

void response_echo() {
    //assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
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

void write32(volatile uint32_t* buffer, unsigned int index, uint32_t item) {
    buffer[index] = item >> 16;
    buffer[index + 1] = item % (1 << 16);
}

void response_status() {
    //assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    int bodySize = 13;
    outBody[0] = state;
    write32(outBody, 1, packetsReceived);
    write32(outBody, 3, wordsReceived);
    write32(outBody, 5, timeAlive);
    write32(outBody, 7, lastLoopTime);
    write32(outBody, 9, maxLoopTime);
    write32(outBody, 11, errors);
    if (DEBUG && shouldClearSendBuffer) {
        assert(outBody[bodySize-1] != 0xbeef);
        assert(outBody[bodySize] == 0xbeef);
    }
    setupTransmission(RESPONSE_OK, bodySize);
}

void responseImuDump() {
    setupTransmissionWithChecksum(RESPONSE_PID_DATA, IMU_DATA_DUMP_SIZE * sizeof(pidSample) * 8 / 16, imuPacketChecksum, imuDumpPacket);
}

void responseBadPacket(uint16_t flag) {
    errors++;
    assert(!transmitting);
    debugPrintf("Bad packet: flag %d\n", flag);
    unsigned int bodySize = 4;
    outBody[0] = flag;
    for (unsigned int i = 1; i < bodySize; i++) {
        outBody[i] = 0;
    }
    setupTransmission(RESPONSE_BAD_PACKET, bodySize);
}

void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint32_t *packetBuffer) {
    assert(!transmitting);
    assert(header <= MAX_HEADER);
    assert(bodyLength <= 500);
    assert(bodyLength > 0);
    currentlyTransmittingPacket = packetBuffer;
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    packetBuffer[0] = FIRST_WORD;
    packetBuffer[1] = transmissionSize;
    packetBuffer[2] = ~transmissionSize;
    packetBuffer[3] = header;
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN] != 0xbeef);
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN + bodyLength - 1] != 0xbeef);
    uint16_t checksum = bodyChecksum + transmissionSize + header;
    packetBuffer[transmissionSize - 2] = checksum;
    packetBuffer[transmissionSize - 1] = LAST_WORD;
    //debugPrintf("Sending checksum: %x\n", checksum);
}

// Call this after body of transmission is filled
void setupTransmissionWithBuffer(uint16_t header, unsigned int bodyLength, volatile uint32_t *packetBuffer) {
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    uint16_t bodyChecksum = transmissionSize + header;
    for (unsigned int i = OUT_PACKET_BODY_BEGIN; i < (unsigned int) (transmissionSize - OUT_PACKET_BODY_END_SIZE); i++) {
      bodyChecksum += packetBuffer[i];
    }
    setupTransmissionWithChecksum(header, bodyLength, bodyChecksum, packetBuffer);
}

void setupTransmission(uint16_t header, unsigned int bodyLength){
    setupTransmissionWithBuffer(header, bodyLength, outData);
}

void clearBuffer(void) {
    noInterrupts();
    transmitting = false;
    dma_rx.disable();
    dma_tx.disable();
    dma_rx.destinationBuffer((uint16_t*) packet, PACKET_SIZE * 2);
    dma_tx.sourceBuffer((uint32_t *) beef_only, PACKET_SIZE * 2);
    dma_tx.triggerAtTransfersOf(dma_rx);
    (void) SPI1_POPR; (void) SPI1_POPR; SPI1_SR |= SPI_SR_RFDF;
    dma_tx.enable();
    dma_rx.enable();
    interrupts();
    /*
    if (packetPointer != 0 && packetPointer != PACKET_SIZE) {
    debugPrintf("Clearing %d bytes of data\n", packetPointer);
    }
    noInterrupts();
    packetPointer = 0;
    outPointer = 0;
    transmissionSize = 0;
    transmitting = false;
    interrupts();
    */
}
