#include "packet.h"
#include "main.h"
#include "states.h"
#include "pidData.h"
#include<DMAChannel.h>

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
volatile uint16_t *currentlyTransmittingPacket = outData; // One doesn't always have to supply outData as the packet buffer
volatile unsigned int outPointer = 0;
volatile uint16_t transmissionSize = 0;
volatile bool transmitting = false;

bool shouldClearSendBuffer = false;

// Local functions
void response_echo();
void response_status();
void responseBadPacket(uint16_t flag);
void create_response();
void responseImuDump();
void setupTransmission(uint16_t header, unsigned int bodyLength);
void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint16_t *packetBuffer);

#define NDAT 10
uint16_t xx[NDAT],yy[NDAT];
unsigned int numReceived = 0;
unsigned int numSent = 0;

void dma_ch0_isr(void)
{ DMA_CINT=0;
  DMA_CDNE=0;
  numSent++;
  debugPrintf("Hey\n");
}

void setDest(void);
void dma_ch1_isr(void)
{ DMA_CINT=1;
  DMA_CDNE=1;
  SPI1_MCR |= SPI_MCR_HALT | SPI_MCR_MDIS;
  setDest();
    int ii;
    Serial.printf("Slave: ");

  for(ii=0;ii<8;ii++) {
    if (yy[ii] != 0xffff) {
      Serial.printf("%x ",xx[ii]); Serial.println();
    }
  }
  numReceived++;
}

void setDest(void) {
     for(int ii=0;ii<NDAT;ii++) xx[ii]=yy[ii];
// set transmit
      DMA_TCD0_DADDR=&SPI1_PUSHR_SLAVE;
        DMA_TCD0_DOFF=0;
        DMA_TCD0_DLASTSGA= 0;

        DMA_TCD0_ATTR=1<<8|1;
        DMA_TCD0_NBYTES_MLNO=2;

        DMA_TCD0_SADDR=xx;
        DMA_TCD0_SOFF=2;
        DMA_TCD0_SLAST=-2*NDAT;

        DMA_TCD0_CITER_ELINKNO = DMA_TCD0_BITER_ELINKNO=NDAT;

        DMA_TCD0_CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

  DMAMUX0_CHCFG0 = DMAMUX_DISABLE;
  DMAMUX0_CHCFG0 = DMAMUX_SOURCE_SPI1_TX | DMAMUX_ENABLE;
}

void startXfer(void)
{
  //spi SETUP
  /*SPI1_MCR =  //SPI_MCR_MDIS |   // module disable
  SPI_MCR_HALT |   // stop transfer
  SPI_MCR_PCSIS(0x1F); // set all inactive states high
  SPI1_MCR |= SPI_MCR_CLR_TXF;
  SPI1_MCR |= SPI_MCR_CLR_RXF;

  SPI1_CTAR0_SLAVE = SPI_CTAR_FMSZ(15);

  SPI1_MCR = 0;

  // start SPI
   SPI1_RSER = SPI_RSER_TFFF_DIRS | SPI_RSER_TFFF_RE | // transmit fifo fill flag to DMA
    SPI_RSER_RFDF_DIRS | SPI_RSER_RFDF_RE;  // receive fifo drain flag to DMA*/

// set receive
  DMA_TCD1_SADDR=&SPI1_POPR;
  DMA_TCD1_SOFF=0;
  DMA_TCD1_SLAST= 0;

  DMA_TCD1_ATTR=1<<8|1;
  DMA_TCD1_NBYTES_MLNO=2;

  DMA_TCD1_DADDR=yy;
  DMA_TCD1_DOFF=2;
  DMA_TCD1_DLASTSGA=-2*NDAT;

  DMA_TCD1_CITER_ELINKNO = DMA_TCD1_BITER_ELINKNO=NDAT;

  DMA_TCD1_CSR = DMA_TCD_CSR_INTMAJOR | DMA_TCD_CSR_DREQ;

  DMAMUX0_CHCFG1 = DMAMUX_DISABLE;
  DMAMUX0_CHCFG1 = DMAMUX_SOURCE_SPI1_RX | DMAMUX_ENABLE;

  //start DMA
        DMA_SERQ = 0;
  NVIC_ENABLE_IRQ(IRQ_DMA_CH0);
        DMA_SERQ = 1;
  NVIC_ENABLE_IRQ(IRQ_DMA_CH1);
}

void dma_slave_setup(void)
{
    SIM_SCGC6 |= SIM_SCGC6_SPI1;
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX;
    SIM_SCGC7 |= SIM_SCGC7_DMA;

    DMA_CR = 0;

    /*CORE_PIN31_CONFIG = PORT_PCR_MUX(2); // CS
    CORE_PIN32_CONFIG = PORT_PCR_MUX(2); // SCK
    CORE_PIN1_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(2); // DOUT
    CORE_PIN0_CONFIG = PORT_PCR_MUX(2); // DIN*/

    int ii;

   for(ii=0;ii<NDAT;ii++) yy[ii]=0;
   for(ii=0;ii<NDAT;ii++) xx[ii]=0xabcd;

}

DMAChannel dma_rx;
DMAChannel dma_tx;
const uint16_t buffer_size = 600;
uint16_t spi_rx_dest[buffer_size];
uint16_t spi_tx_out[buffer_size];
uint32_t spi_tx_src = SPI_PUSHR_PCS(0) | SPI_PUSHR_CTAS(0) | 0x4242;
void setup_dma_receive(void) {
    dma_rx.source((uint16_t&) KINETISK_SPI1.POPR);
    dma_rx.destinationBuffer((uint16_t*) spi_rx_dest, sizeof(spi_rx_dest));
    dma_rx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI1_RX);

    spi_rx_dest[0] = 0xbeef;
    dma_tx.source((uint32_t&) spi_tx_src);
    dma_tx.destination((uint32_t&) KINETISK_SPI1.PUSHR); // SPI1_PUSHR_SLAVE
    dma_tx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI1_RX);

    /*auto kinetis_spi_cs = spi.setCS(spi_cs_pin);
    spi_tx_src[0] = SPI_PUSHR_PCS(kinetis_spi_cs) | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | 0x4242;
    spi_tx_src[1] = SPI_PUSHR_PCS(kinetis_spi_cs) | SPI_PUSHR_EOQ | SPI_PUSHR_CTAS(1) | 0x4343u;
    dma_tx.TCD->SADDR = spi_tx_src;
    dma_tx.TCD->ATTR_SRC = 2; // 32-bit read from source
    dma_tx.TCD->SOFF = 4;
    // transfer both 32-bit entries in one minor loop
    dma_tx.TCD->NBYTES = 8;
    dma_tx.TCD->SLAST = -sizeof(spi_tx_src); // go back to beginning of buffer
    dma_tx.TCD->DADDR = &KINETISK_SPI0.PUSHR;
    dma_tx.TCD->DOFF = 0;
    dma_tx.TCD->ATTR_DST = 2; // 32-bit write to dest
    // one major loop iteration
    dma_tx.TCD->BITER = 1;
    dma_tx.TCD->CITER = 1;
    dma_tx.triggerAtCompletionOf(dma_start_spi);*/

    SPI1_RSER = SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS; // DMA on receive FIFO drain flag
    SPI1_SR = 0xFF0F0000;
    dma_rx.enable();
    dma_tx.enable();
}

void packet_setup(void) {
    assert(outPointer == 0);
    assert(transmissionSize == 0);
    assert(transmitting == false);
    SPI_SLAVE.begin_SLAVE(SCK1, MOSI1, MISO1, T3_SPI1_CS0);
    SPI_SLAVE.setCTAR_SLAVE(16, T3_SPI_MODE0);
    setup_dma_receive();
    //dma_slave_setup();
    //startXfer();
    /*NVIC_ENABLE_IRQ(IRQ_SPI1);
    NVIC_SET_PRIORITY(IRQ_SPI1, 16);

    pinMode(PACKET_RECEIVED_TRIGGER, OUTPUT);
    digitalWrite(PACKET_RECEIVED_TRIGGER, LOW);

    // Not sure what priority this should be; this shouldn't fire at the same time as IRQ_SPI0
    attachInterrupt(SLAVE_CHIP_SELECT, clearBuffer, FALLING);
    //attachInterrupt(PACKET_RECEIVED_PIN, handlePacket, FALLING);

    // Low priority for pin 26 -- packet received interrupt
    NVIC_SET_PRIORITY(IRQ_PORTE, 144);*/
}

uint16_t getHeader() {
    return currentlyTransmittingPacket[3];
}

//Interrupt Service Routine to handle incoming data
void spi1_isr(void) {
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
}

void packetReceived() {
  assert(packetPointer == PACKET_SIZE);
  packetsReceived++;

  handlePacket();
}

void handlePacket() {
  unsigned int startTimePacketPrepare = micros();
  //debugPrintln("Received a packet!");
  assert(packetPointer == PACKET_SIZE);

  // Check for erroneous data
  if (transmitting || outPointer != 0) {
    debugPrintln("Error: I'm already transmitting!");
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
    assert(packetPointer == PACKET_SIZE);
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

void clearSendBuffer() { // Just for debugging purposes
    if (DEBUG && shouldClearSendBuffer) {
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

void write32(volatile uint16_t* buffer, unsigned int index, uint32_t item) {
    *((volatile uint32_t *) &buffer[index]) = item;
}

void response_status() {
    assert(packetPointer == PACKET_SIZE);
    assert(!transmitting);
    clearSendBuffer();
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

void setupTransmissionWithChecksum(uint16_t header, unsigned int bodyLength, uint16_t bodyChecksum, volatile uint16_t *packetBuffer) {
    assert(!transmitting);
    assert(header <= MAX_HEADER);
    assert(bodyLength <= 500);
    assert(bodyLength > 0);
    currentlyTransmittingPacket = packetBuffer;
    outPointer = 0;
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
    transmitting = true;
}

// Call this after body of transmission is filled
void setupTransmissionWithBuffer(uint16_t header, unsigned int bodyLength, volatile uint16_t *packetBuffer) {
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
