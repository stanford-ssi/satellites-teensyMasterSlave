#include "dma.h"
#include <array>
#include <DMAChannel.h>

const uint8_t spi_cs_pin = 15;   // pin 15 SPI0 chip select
const uint8_t trigger_pin = 18;  // PTB11.  This pin will receive conversion ready signal

DMAChannel dma_start_spi;
DMAChannel dma_rx;
DMAChannel dma_tx;
std::array<uint32_t, 2> spi_tx_src;
std::array<volatile uint32_t, DMASIZE> spi_rx_dest;
uint32_t rx_begin = uint32_t(spi_rx_dest.data());

SPISettings spi_settings(10, MSBFIRST, SPI_MODE0);
uint32_t start_spi_sr = 0xFF0F0000;

uint32_t backOfBuffer = 0;

uint32_t dmaGetOffset() {
    uint32_t dma_rx_pos = (uint32_t) dma_rx.destinationAddress();
    uint32_t offset;
    assert(dma_rx_pos >= rx_begin);
    uint32_t index = dma_rx_pos - rx_begin;
    if (index >= backOfBuffer) {
        offset = index - backOfBuffer;
    } else {
        assert(index + (DMA_SAMPLE_DEPTH * DMASIZE) >= backOfBuffer);
        offset = index + (DMA_SAMPLE_DEPTH * DMASIZE) - backOfBuffer;
    }
    offset -= offset % 4;
    assert(offset % 4 == 0);
    offset /= 4; // Convert byte to uint32
    //debugPrintf("Dma at %d, rx at %d, begin %d, index %d, offset %d\n", dma_rx_pos, rx_begin, backOfBuffer, index, offset);
    return offset;
}

bool dmaSampleReady() {
    return dmaGetOffset() >= 4;
}

volatile uint32_t* dmaGetSample() {
    assert(dmaSampleReady());
    volatile uint32_t* toReturn = &spi_rx_dest[backOfBuffer];
    backOfBuffer = (backOfBuffer + DMA_SAMPLE_NUMAXES * DMA_SAMPLE_DEPTH) % (DMA_SAMPLE_DEPTH * DMASIZE);
    return toReturn;
}

void startSampling() { // Clears out old samples so the first sample you read is fresh
    uint32_t offset = dmaGetOffset();
    offset -= offset % DMA_SAMPLE_DEPTH;
    assert(offset % DMA_SAMPLE_DEPTH == 0);
    backOfBuffer = offset;
}

void dmaSetup() {
    Serial.println("Starting.");

    SPI.begin();

    pinMode(spi_cs_pin, OUTPUT);
    digitalWriteFast(spi_cs_pin, HIGH);
    auto kinetis_spi_cs = SPI.setCS(spi_cs_pin);
    spi_tx_src = {
        SPI_PUSHR_PCS(kinetis_spi_cs) | SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | 0x4242,
        SPI_PUSHR_PCS(kinetis_spi_cs) | SPI_PUSHR_EOQ | SPI_PUSHR_CTAS(1) | 0x4343u,
    };

    SPI.beginTransaction(spi_settings);

    // for testing, pin 31 connected to 32, send t in serial monitor to initiate SPI transfer
    const uint8_t trigger_pin_out = 31;
    pinMode(trigger_pin_out, OUTPUT);
    digitalWriteFast(trigger_pin_out, LOW);

    pinMode(trigger_pin, INPUT_PULLUP);
    volatile uint32_t *pin_config = portConfigRegister(trigger_pin);
    *pin_config |= PORT_PCR_IRQC(0b0010); // DMA on falling edge

    dma_start_spi.sourceBuffer(&start_spi_sr, sizeof(start_spi_sr));
    dma_start_spi.destination(KINETISK_SPI0.SR);
    // triggered by pin 32, port B
    dma_start_spi.triggerAtHardwareEvent(DMAMUX_SOURCE_PORTB);

    dma_tx.TCD->SADDR = spi_tx_src.data();
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
    dma_tx.triggerAtCompletionOf(dma_start_spi);

    dma_rx.source((uint16_t&) KINETISK_SPI0.POPR);
    dma_rx.destinationBuffer((uint16_t*) spi_rx_dest.data(), sizeof(spi_rx_dest));
    dma_rx.triggerAtHardwareEvent(DMAMUX_SOURCE_SPI0_RX);

    SPI0_RSER = SPI_RSER_RFDF_RE | SPI_RSER_RFDF_DIRS; // DMA on receive FIFO drain flag
    SPI0_SR = 0xFF0F0000;

    dma_rx.enable();
    dma_tx.enable();
    dma_start_spi.enable();
}
