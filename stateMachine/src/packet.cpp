#include "packet.h"
#include "main.h"
#include "states.h"
#include "pidData.h"
#include <DMAChannel.h>
#include "tracking.h"

SpiSlave spiSlave;

void receivedPacketIsr(void)
{
    spiSlave.receivedPacket();
}

void clearBufferIsr(void) {
    spiSlave.clearBuffer();
}

SpiSlave::SpiSlave(void) {
    for (int i = 0; i < 11; i++) {
        beef_only[i] = 0xabcd;
    }
    for (int i = 0; i < buffer_size; i++) {
        spi_tx_out[i] = 0xffff0100 + i;
    }
    for (int i = 0; i < ABCD_BUFFER_SIZE; i++) {
        spi_tx_out[i] = 0xabcd;
    }
    for (int i = 0; i < buffer_size; i++) {
        packet[i] = 0;
    }
}

void SpiSlave::receivedPacket(void) {
    noInterrupts();
    if(!(spiSlave.packet[0] == 0x1234)) {
        errors++;
    }
    dma_rx.disable();
    dma_tx.disable();
    dma_rx.clearInterrupt();
    if (!transmitting) {
        packetReceived();
        const uint32_t sizeOfBuffer = PID_DATA_DUMP_SIZE_UINT16 + PID_HEADER_SIZE + OUT_PACKET_OVERHEAD + ABCD_BUFFER_SIZE + 2;
        (void) assert(sizeOfBuffer <= 512);
        dma_rx.destinationBuffer((uint16_t*) packet + PACKET_SIZE, sizeof(uint16_t) * sizeOfBuffer);
        dma_tx.sourceBuffer((uint32_t *) currentlyTransmittingPacket - ABCD_BUFFER_SIZE, sizeof(uint32_t) * sizeOfBuffer);
        dma_tx.triggerAtTransfersOf(dma_rx);
        transmitting = true;
        dma_tx.enable();
        dma_rx.enable();
    } else {
        if (((state == TRACKING_STATE) || (state == CALIBRATION_STATE)) && getHeader() == RESPONSE_PID_DATA) {
            assert(pidPacketReady);
            pidPacketSent();
        }
    }
    interrupts();
}

extern pidDumpPacket_t pidDumpPacket;
void SpiSlave::setup_dma_receive(void) {
    for (unsigned int i = 0; i < sizeof(pidDumpPacket.body) / 2; i++) {
        ((uint16_t *) pidDumpPacket.body)[i] = 0xabcd;
    }
    dma_rx.source((uint16_t&) KINETISK_SPI1.POPR);
    dma_rx.destinationBuffer((uint16_t*) packet, PACKET_SIZE * 2);
    dma_rx.disableOnCompletion();
    dma_rx.interruptAtCompletion();
    dma_rx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI1_RX);
    dma_rx.attachInterrupt(receivedPacketIsr);

    dma_tx.sourceBuffer((uint32_t *) beef_only, PACKET_SIZE * 2);
    dma_tx.destination(KINETISK_SPI1.PUSHR); // SPI1_PUSHR_SLAVE
    dma_tx.disableOnCompletion();
    dma_tx.triggerAtTransfersOf(dma_rx);

    SPI1_RSER = SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS; // DMA on receive FIFO drain flag
    SPI1_SR = 0xFF0F0000;

    dma_rx.enable();
    dma_tx.enable();
}

void SpiSlave::packet_setup(void) {
    assert(transmissionSize == 0);
    assert(transmitting == false);
    SPI_SLAVE_T3.begin_SLAVE(SCK1, MOSI1, MISO1, T3_SPI1_CS0);
    SPI_SLAVE_T3.setCTAR_SLAVE(16, T3_SPI_MODE0);
    setup_dma_receive();

    attachInterrupt(SLAVE_CHIP_SELECT, clearBufferIsr, FALLING);
}

uint16_t SpiSlave::getHeader() {
    return currentlyTransmittingPacket[3];
}

void SpiSlave::packetReceived() {
  packetsReceived++;

  handlePacket();
}

void SpiSlave::handlePacket() {
  // Check for erroneous data
  if (transmitting) {
    debugPrintln("Error: I'm already transmitting!");
    errors++;
    // No response because we're presumably already transmitting
    // responseBadPacket(INTERNAL_ERROR);
    return;
  }

  if (packet[0] != FIRST_WORD || packet[PACKET_SIZE - 1] != LAST_WORD) {
    /*debugPrintf("Invalid packet endings: ");
    for (int i = 0; i < PACKET_SIZE; i++) {
        debugPrintf("%x ", packet[i]);
    }
    debugPrintln("");*/
    errors++;
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

void SpiSlave::create_response() {
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
        uint16_t bufferSelect = packet[2];
        uint16_t frequency = packet[3];
        uint16_t amplitude = packet[4];
        if (bufferSelect >= mirrorDriver.numMirrorBuffers || frequency == 0 || frequency > 1000 || amplitude > 1000) {
            debugPrintf("bufferSelect %d frequency %d\n", bufferSelect, frequency);
            responseBadPacket(INVALID_COMMAND);
        } else {
            previousState = state;
            mirrorDriver.selectMirrorBuffer(bufferSelect, frequency, amplitude);
            changingState = true;
            state = CALIBRATION_STATE;
            response_status();
        }

    } else if (command == COMMAND_REPORT_TRACKING) {
        if (!(state == TRACKING_STATE || state == CALIBRATION_STATE)) {
            responseBadPacket(INVALID_COMMAND);
        } else if (!pidPacketReady) {
            debugPrintf("Front of buf %d back %d samples sent %d\n", pidDataPointer, pidSentDataPointer, pidSamplesSent);
            responseBadPacket(DATA_NOT_READY);
        } else {
            responsePidDump();
        }
    } else if (command == COMMAND_PROBE_MEMORY) {
        response_probe();
    } else if (command == COMMAND_WRITE_MEMORY) {
        response_write();
    } else if (command == COMMAND_SET_CONSTANT) {
        response_set_constant();
    } else {
        debugPrintf("Command out of range\n");
        responseBadPacket(INVALID_COMMAND);
    }
}

void SpiSlave::response_echo() {
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

void SpiSlave::write32(volatile uint32_t* buffer, unsigned int index, uint32_t item) {
    buffer[index] = item >> 16;
    buffer[index + 1] = item % (1 << 16);
}

void SpiSlave::write64(volatile uint32_t* buffer, unsigned int index, uint64_t item) {
    buffer[index] = item >> 48;
    buffer[index + 1] = (item >> 32) % (1 << 16);
    buffer[index + 2] = (item >> 16) % (1 << 16);
    buffer[index + 3] = item  % (1 << 16);
}

void SpiSlave::response_status() {
    //assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    int bodySize = 14;

    write64(outBody, 0, pointer.numLockedOn);
    write64(outBody, 4, pointer.totalPowerReceivedBeforeIncoherent);
    write64(outBody, 8, pointer.totalPowerReceived);
    write32(outBody, 12, pointer.samplesProcessed);
    if (DEBUG && shouldClearSendBuffer) {
        assert(outBody[bodySize-1] != 0xbeef);
        assert(outBody[bodySize] == 0xbeef);
    }
    setupTransmission(RESPONSE_OK, bodySize);
}

void SpiSlave::response_probe() {
    uint16_t probe_size = packet[2];
    uint32_t address = (packet[3] << 16) + packet[4];
    assert(!transmitting);
    uint64_t toReturn = 0;
    if (probe_size == 8) {
        toReturn = * ((uint8_t *) address); // This is safe lol
    } else if (probe_size == 16) {
        toReturn = * ((uint16_t *) address);
    } else if (probe_size == 32) {
        toReturn = * ((uint32_t *) address);
    } else if (probe_size == 64) {
        toReturn = * ((uint64_t *) address);
    } else {
        // debugPrintf("%x %x %x %x %x %x %x\n", outBody[-2], outBody[-1], outBody[0], outBody[1], outBody[2], outBody[3], outBody[4]);
        responseBadPacket(INVALID_COMMAND);
        return;
    }
    int bodySize = 4;
    write64(outBody, 0, toReturn);
    setupTransmission(RESPONSE_PROBE, bodySize);
}

void SpiSlave::response_write() {
    uint16_t probe_size = packet[2];
    uint32_t address = (packet[3] << 16) + packet[4];
    uint32_t to_write = (packet[5] << 16) + packet[6];
    uint16_t checksum = packet[7];
    if (checksum != (uint16_t) (packet[5] + packet[6])) {
        responseBadPacket(INVALID_CHECKSUM);
        return;
    }
    assert(!transmitting);
    uint64_t toReturn = 0;
    if (probe_size == 8) {
        toReturn = * ((uint8_t *) address); // This is safe lol
        * ((uint8_t *) address) = to_write;
    } else if (probe_size == 16) {
        toReturn = * ((uint16_t *) address);
        * ((uint16_t *) address) = to_write;
    } else if (probe_size == 32) {
        toReturn = * ((uint32_t *) address);
        * ((uint32_t *) address) = to_write;
    } else {
        debugPrintf("%d\n", probe_size);
        responseBadPacket(INVALID_COMMAND);
        return;
    }
    int bodySize = 4;
    write64(outBody, 0, toReturn);
    setupTransmission(RESPONSE_PROBE, bodySize);
}

void SpiSlave::response_set_constant() {
    uint16_t constant_number = packet[2];
    uint16_t constant_type = packet[3];
    int32_t desired_value = (packet[4] << 16) + packet[5];
    assert(!transmitting);
    uint32_t toReturn = 0;
    volatile double garbage = 0;
    volatile double* doubleAddress = &garbage; // Just in case not initialized
    volatile uint32_t garbage2 = 0;
    volatile uint32_t* uintAddress = &garbage2;
    if (constant_type == 0) { // int
        if (constant_number == 0) {
            toReturn = 0;
            (void) desired_value;
        } else if (constant_number == 1) {
            uintAddress = &packetsReceived; // Not really used for anything
        } else {
            responseBadPacket(INVALID_COMMAND);
            return;
        }
    } else if (constant_type == 1) { // float
        if (constant_number == 50) {
            toReturn = 0;
            (void) desired_value;
        } else if (constant_number == 51) {
            doubleAddress = &pid._dt;
        } else if (constant_number == 52) {
           doubleAddress = &pid._Kp;
        } else if (constant_number == 53) {
           doubleAddress = &pid._Kd;
        } else if (constant_number == 54) {
           doubleAddress = &pid._Kd;
        } else if (constant_number == 55) {
           doubleAddress = &pid._Ki;
        } else if (constant_number == 56) {
          doubleAddress = &pid._min;
        } else if (constant_number == 57) {
          doubleAddress = &pid._max;
        } else if (constant_number == 58) {
          doubleAddress = &pointer.theta;
        } else {
           responseBadPacket(INVALID_COMMAND);
           return;
        }
    } else {
        responseBadPacket(INVALID_COMMAND);
        return;
    }

    if (constant_type == 0) {
        toReturn = *uintAddress;
        *uintAddress = desired_value;
    } else if (constant_type == 1) {
        toReturn = *doubleAddress * 1000;
        *doubleAddress = desired_value / 1000.0;
    }

    int bodySize = 4;
    write64(outBody, 0, toReturn);
    setupTransmission(RESPONSE_PROBE, bodySize);
}

void SpiSlave::responsePidDump() {
    setupTransmissionWithChecksum(RESPONSE_PID_DATA, PID_DATA_DUMP_SIZE_UINT16 + PID_HEADER_SIZE, pidPacketChecksum, pidDumpPacketUints);
}

void SpiSlave::responseBadPacket(uint16_t flag) {
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

void SpiSlave::setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint32_t *packetBuffer) {
    assert(!transmitting);
    assert(header <= MAX_HEADER);
    assert(bodyLength <= 500);
    //assert(bodyLength > 0);
    currentlyTransmittingPacket = packetBuffer;
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    packetBuffer[0] = FIRST_WORD;
    packetBuffer[1] = transmissionSize;
    packetBuffer[2] = ~transmissionSize;
    packetBuffer[3] = header;
    packetBuffer[4] = state;
    write32(packetBuffer, 5, packetsReceived);
    write32(packetBuffer, 7, timeAlive);
    write32(packetBuffer, 9, lastLoopTime);
    write32(packetBuffer, 11, maxLoopTime);
    write32(packetBuffer, 13, errors);
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN] != 0xbeef);
    assert(packetBuffer[OUT_PACKET_BODY_BEGIN + bodyLength - 1] != 0xbeef);
    uint16_t checksum = bodyChecksum;
    for (int i = 1; i < OUT_PACKET_BODY_BEGIN; i++) {
        checksum += packetBuffer[i];
    }
    packetBuffer[transmissionSize - 2] = checksum;
    packetBuffer[transmissionSize - 1] = LAST_WORD;
}

// Call this after body of transmission is filled
void SpiSlave::setupTransmissionWithBuffer(uint16_t header, unsigned int bodyLength, volatile uint32_t *packetBuffer) {
    transmissionSize = bodyLength + OUT_PACKET_OVERHEAD;
    uint16_t bodyChecksum = 0;
    for (unsigned int i = OUT_PACKET_BODY_BEGIN; i < (unsigned int) (transmissionSize - OUT_PACKET_BODY_END_SIZE); i++) {
        bodyChecksum += packetBuffer[i];
    }
    setupTransmissionWithChecksum(header, bodyLength, bodyChecksum, packetBuffer);
}

void SpiSlave::setupTransmission(uint16_t header, unsigned int bodyLength){
    setupTransmissionWithBuffer(header, bodyLength, outData);
}

void SpiSlave::clearBuffer(void) {
    noInterrupts();
    dma_rx.disable();
    dma_tx.disable();
    (void) SPI1_POPR; (void) SPI1_POPR; SPI1_SR |= SPI_SR_RFDF;
    transmitting = false;
    dma_rx.destinationBuffer((uint16_t*) packet, PACKET_SIZE * 2);
    dma_tx.sourceBuffer((uint32_t *) beef_only, PACKET_SIZE * 2);
    dma_tx.triggerAtTransfersOf(dma_rx);
    dma_tx.enable();
    dma_rx.enable();
    interrupts();
}
