#include <t3spi.h>

#ifndef PACKET_H
#define PACKET_H
//Initialize T3SPI class as SPI_SLAVE
extern T3SPI SPI_SLAVE;

#define SLAVE_CHIP_SELECT 10
#define PACKET_RECEIVED_PIN 26
// This pin will loopback to PACKET_RECEIVED_PIN to signal a packet is received
#define PACKET_RECEIVED_TRIGGER 27

// Always have first word in packet be constant for alignment checking
#define FIRST_WORD 0x1234
// Always have last word in packet be constant for alignment checking
#define LAST_WORD 0x4321
// 0xabcd is more useful than 0 because it indicates payload is alive
#define EMPTY_WORD 0xabcd

#define PACKET_SIZE 5 // Fixed size incoming packet
// First word in packet body; word 0 is FIRST_WORD
#define PACKET_BODY_BEGIN 1
// Exclusive; last two words are checksum and LAST_WORD
#define PACKET_BODY_END (PACKET_SIZE - 2)
#define BODY_LENGTH (PACKET_BODY_END - PACKET_BODY_BEGIN)
#define PACKET_OVERHEAD (PACKET_SIZE - BODY_LENGTH)

#define OUT_PACKET_BODY_BEGIN 3
#define OUT_PACKET_BODY_END_SIZE 2
#define OUT_PACKET_OVERHEAD (OUT_PACKET_BODY_END_SIZE + OUT_PACKET_BODY_BEGIN)

// Headers
#define MIN_HEADER 0
#define MAX_HEADER 1
#define RESPONSE_OK 0
#define RESPONSE_BAD_PACKET 1

#define INVALID_BORDER 0
#define INVALID_CHECKSUM 1
#define INTERNAL_ERROR 2

void packet_setup(void);
void setupTransmission(uint16_t header, unsigned int bodyLength);
void response_echo();
void responseBadPacket(uint16_t flag);

#endif
