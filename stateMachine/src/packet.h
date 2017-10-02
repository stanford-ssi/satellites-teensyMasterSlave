#ifndef PACKET_H
#define PACKET_H
#include <t3spi.h>
#include "DMAChannel.h"
#include "pid.h"

class SpiSlave {
public:
    SpiSlave();

    //Initialize T3SPI class as SPI_SLAVE
    T3SPI SPI_SLAVE_T3;
    volatile uint16_t transmissionSize = 0;
    volatile bool transmitting = false;
    // Very little effort is made to prevent these from overflowing
    // They will only be used for basic telemetry
    volatile uint32_t packetsReceived = 0;
    volatile uint32_t wordsReceived = 0;

    volatile uint16_t SLAVE_CHIP_SELECT = 31;
    // Always have first word in packet be constant for alignment checking
    const uint16_t FIRST_WORD = 0x1234;
    // Always have last word in packet be constant for alignment checking
    const uint16_t LAST_WORD = 0x4321;
    // 0xabcd is more useful than 0 because it indicates payload is alive
    const uint16_t EMPTY_WORD = 0xabcd;

    const uint16_t PACKET_SIZE = 10; // Fixed size incoming packet
    // Index of first word in packet body; word 0 is FIRST_WORD
    const uint16_t PACKET_BODY_BEGIN = 1; // First word in body is command number
    // Exclusive; last two words are checksum and LAST_WORD
    const uint16_t PACKET_BODY_END = (PACKET_SIZE - 2);
    const uint16_t BODY_LENGTH = (PACKET_BODY_END - PACKET_BODY_BEGIN);
    const uint16_t PACKET_OVERHEAD = (PACKET_SIZE - BODY_LENGTH);

    const static uint16_t ABCD_BUFFER_SIZE = 2;
    const static uint16_t OUT_PACKET_BODY_BEGIN = 15;
    const static uint16_t OUT_PACKET_BODY_END_SIZE = 2;
    const static uint16_t OUT_PACKET_OVERHEAD = (OUT_PACKET_BODY_END_SIZE + OUT_PACKET_BODY_BEGIN);

    // Commands
    const uint16_t MIN_COMMAND = 0;
    const uint16_t COMMAND_ECHO = 0;
    const uint16_t COMMAND_STATUS = 1;
    const uint16_t COMMAND_IDLE = 2;
    const uint16_t COMMAND_SHUTDOWN = 3;
    const uint16_t COMMAND_IMU = 4;
    const uint16_t COMMAND_IMU_DUMP = 5;
    const uint16_t COMMAND_CALIBRATE = 6;
    const uint16_t COMMAND_POINT_TRACK = 7;
    const uint16_t COMMAND_REPORT_TRACKING = 8;
    const uint16_t COMMAND_PROBE_MEMORY = 9;
    const uint16_t COMMAND_WRITE_MEMORY = 10;
    const uint16_t COMMAND_SET_CONSTANT = 11;
    const uint16_t MAX_COMMAND = 11;

    // Response Headers
    const uint16_t MIN_HEADER = 0;
    const uint16_t RESPONSE_OK = 0;
    const uint16_t RESPONSE_BAD_PACKET = 1;
    const uint16_t RESPONSE_PID_DATA = 3;
    const uint16_t RESPONSE_ADCS_REQUEST = 4;
    const uint16_t RESPONSE_PROBE = 5;
    const uint16_t MAX_HEADER = 5;

    // Error numbers
    const uint16_t INVALID_BORDER = 0;
    const uint16_t INVALID_CHECKSUM = 1;
    const uint16_t INTERNAL_ERROR = 2;
    const uint16_t INVALID_COMMAND = 3;
    const uint16_t DATA_NOT_READY = 4;
    const uint16_t STATE_NOT_READY = 5;

    void packet_setup(void);
    void receivedPacket(void);
    void clearBuffer(void);
    void packetReceived(void);

private:
    DMAChannel dma_rx;
    DMAChannel dma_tx;
    uint32_t beef_only[11];
    const static uint16_t buffer_size = 750;
    uint32_t spi_tx_out[buffer_size];
    uint16_t packet[buffer_size];

    // Outgoing
    volatile uint32_t *outData = spi_tx_out + ABCD_BUFFER_SIZE;
    volatile uint32_t *outBody = outData + OUT_PACKET_BODY_BEGIN;
    volatile uint32_t *currentlyTransmittingPacket = outData; // One doesn't always have to supply outData as the packet buffer

    bool shouldClearSendBuffer = false;

    // Local functions
    void write32(volatile uint32_t* buffer, unsigned int index, uint32_t item);
    void write64(volatile uint32_t* buffer, unsigned int index, uint64_t item);
    void handlePacket();
    void response_echo();
    void response_status();
    void response_probe();
    void response_write();
    void response_set_constant();
    void responseBadPacket(uint16_t flag);
    void create_response();
    void responsePidDump();
    void setupTransmission(uint16_t header, unsigned int bodyLength);
    void setupTransmissionWithBuffer(uint16_t header, unsigned int bodyLength, volatile uint32_t *packetBuffer);
    void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint32_t *packetBuffer);
    uint16_t getHeader(void);
    void setup_dma_receive(void);
};

extern SpiSlave spiSlave;

#endif
