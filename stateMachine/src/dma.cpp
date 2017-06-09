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

uint32_t backOfBuffer = 0; // unsigned char* pointer

uint32_t dmaGetOffset() {
    uint32_t dma_rx_pos = (uint32_t) dma_rx.destinationAddress(); // Pointer, in bytes
    uint32_t offset;
    assert(dma_rx_pos >= rx_begin);
    if (!(dma_rx_pos >= rx_begin)) {
        debugPrintf("Start %d, currently %d\n", rx_begin, dma_rx_pos);
    }

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

volatile adcSample* dmaGetSample() {
    assert(dmaSampleReady());
    volatile uint32_t* toReturn = &spi_rx_dest[backOfBuffer];
    backOfBuffer = (backOfBuffer + DMA_SAMPLE_NUMAXES * DMA_SAMPLE_DEPTH) % (DMA_SAMPLE_DEPTH * DMASIZE);
    return (adcSample *) toReturn;
}

void dmaStartSampling() { // Clears out old samples so the first sample you read is fresh
    uint32_t offset = dmaGetOffset();
    offset -= offset % DMA_SAMPLE_NUMAXES; // Only clear out in increments of DMA_SAMPLE_NUMAXES
    assert(offset % DMA_SAMPLE_NUMAXES == 0);
    assert(backOfBuffer < DMASIZE * DMA_SAMPLE_DEPTH); // Should index somewhere in the buffer
    backOfBuffer = (backOfBuffer + 4 * offset) % (DMASIZE * DMA_SAMPLE_DEPTH);
}

void init_FTM0(){ // code based off of https://forum.pjrc.com/threads/24992-phase-correct-PWM?styleid=2

 FTM0_SC = 0;
 FTM1_SC = 0;

 analogWriteFrequency(22, 5000); // FTM0 channel 0
 analogWriteFrequency(3, 20000); // FTM1 channel 0
 FTM0_POL = 0;                  // Positive Polarity
 FTM0_OUTMASK = 0xFF;           // Use mask to disable outputs
 FTM0_CNTIN = 0;                // Counter initial value
 FTM0_COMBINE = 0x00003333;     // COMBINE=1, COMP=1, DTEN=1, SYNCEN=1
 FTM0_MODE = 0x01;              // Enable FTM0
 FTM0_SYNC = 0x02;              // PWM sync @ max loading point enable
 uint32_t mod = FTM0_MOD;
 uint32_t mod2 = FTM1_MOD;
 Serial.printf("mod %d, mod2 %d\n", mod, mod2);
 Serial.printf("clock source %d, 2 %d\n", FTM0_SC, FTM1_SC);
 FTM0_C0V = mod/4;                  // Combine mode, pulse-width controlled by...
 FTM0_C1V = mod * 3/4;           //   odd channel.
 FTM0_C2V = 0;                  // Combine mode, pulse-width controlled by...
 FTM0_C3V = mod/2;           //   odd channel.
 FTM0_SYNC |= 0x80;             // set PWM value update
 FTM0_C0SC = 0x28;              // PWM output, edge aligned, positive signal
 FTM0_C1SC = 0x28;              // PWM output, edge aligned, positive signal
 FTM0_C2SC = 0x28;              // PWM output, edge aligned, positive signal
 FTM0_C3SC = 0x28;              // PWM output, edge aligned, positive signal
 FTM0_OUTMASK = 0x0;            // Turns on PWM output
 analogWrite(3, 128);           // FTM1 50% duty cycle

 FTM0_CONF = ((FTM0_CONF | FTM_CONF_GTBEEN) & ~(FTM_CONF_GTBEOUT));             // GTBEOUT 0 and GTBEEN 1
 FTM1_CONF = ((FTM1_CONF | FTM_CONF_GTBEEN) & ~(FTM_CONF_GTBEOUT));             // GTBEOUT 0 and GTBEEN 1
 //FTM1_CONF |= FTM_CONF_GTBEOUT;             // GTBEOUT 1

 CORE_PIN22_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;    //config teensy output port pins
 CORE_PIN23_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;   //config teensy output port pins
 CORE_PIN9_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;    //config teensy output port pins
 CORE_PIN10_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;   //config teensy output port pins

 FTM0_CNT = 0;
 FTM1_CNT = 0;

 FTM0_CONF |= FTM_CONF_GTBEOUT;             // GTBEOUT 1
}

void dmaReceiveSetup() {
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

void dmaSetup() {
    debugPrintf("Setup up dma.\n");
    dmaReceiveSetup();
    debugPrintf("Dma setup complete. Setting up ftm timers.\n");
    init_FTM0();
    debugPrintf("FTM timer setup complete.\n");
}
