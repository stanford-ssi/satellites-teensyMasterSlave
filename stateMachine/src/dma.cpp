#include "dma.h"
#include <array>
#include <DMAChannel.h>

const uint8_t spi_cs_pin = 15;   // pin 15 SPI0 chip select
const uint8_t trigger_pin = 18;  // PTB11.  This pin will receive conversion ready signal


SPISettings spi_settings(6250000, MSBFIRST, SPI_MODE0);

uint32_t frontOfBuffer = 0;
uint32_t backOfBuffer = 0;
adcSample adcSamplesRead[DMASIZE + 1];
volatile adcSample nextSample;
volatile unsigned int adcIsrIndex = 0; // indexes into nextSample

mirrorOutput currentOutput;
volatile unsigned int mirrorOutputIndex;

uint32_t dmaGetOffset() {
    uint32_t offset;
    if (frontOfBuffer >= backOfBuffer) {
        offset = frontOfBuffer - backOfBuffer;
    } else {
        assert(frontOfBuffer + DMASIZE >= backOfBuffer);
        offset = frontOfBuffer + DMASIZE - backOfBuffer;
    }
    return offset;
}

bool dmaSampleReady() {
    return dmaGetOffset() >= 1;
}

volatile adcSample* dmaGetSample() {
    assert(dmaSampleReady());
    volatile adcSample* toReturn = &adcSamplesRead[backOfBuffer];
    backOfBuffer = (backOfBuffer + 1) % DMASIZE;
    /*adcSample* sample = (adcSample *) toReturn;
    if (!(sample->axis2 == 0 &&  sample->axis3 == 0 && sample->axis4 == 0)) {
        char debugBuf[40];
        sample->toString(debugBuf, 39);
        debugPrintf("Ruh roh, sample is %s\n", debugBuf);
    }*/
    return toReturn;
}

void dmaStartSampling() { // Clears out old samples so the first sample you read is fresh
    backOfBuffer = frontOfBuffer;
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
 (void) mod2;
 debugPrintf("mod %d, mod2 %d\n", mod, mod2);
 debugPrintf("clock source %d, 2 %d\n", FTM0_SC, FTM1_SC);
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
 debugPrintf("Setting up pins\n");
 CORE_PIN22_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;    //config teensy output port pins
 CORE_PIN23_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;   //config teensy output port pins
 CORE_PIN9_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;    //config teensy output port pins
 CORE_PIN10_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_DSE | PORT_PCR_SRE;   //config teensy output port pins

 FTM0_CNT = 0;
 FTM1_CNT = 0;

 debugPrintf("Starting global clock\n");
 FTM0_CONF |= FTM_CONF_GTBEOUT;             // GTBEOUT 1

}

/* ********* Adc read code ********* */

void spi0_isr(void) {
    noInterrupts();
    assert(adcIsrIndex < (sizeof(adcSample) / (16 / 8)));
    if (!(adcIsrIndex < (sizeof(adcSample) / (16 / 8)))) {
        debugPrintf("Adc isr is %d frontOfBuffer %d last %x %x %x %x\n", adcIsrIndex, frontOfBuffer, adcSamplesRead[DMASIZE].axis1, adcSamplesRead[DMASIZE].axis2, adcSamplesRead[DMASIZE].axis3, adcSamplesRead[DMASIZE].axis4);
        for (int i = -2; i < 3; i++) {
            debugPrintf("i %d, address %p, val %x\n", i, &((uint32_t *) &adcIsrIndex)[i], ((uint32_t *) &adcIsrIndex)[i]);
        }
        return;
    }
    //debugPrintf("spibegin");
    uint16_t spiRead = SPI0_POPR;
    ((volatile uint16_t *) &nextSample)[adcIsrIndex] = spiRead;
    //debugPrintf("%p %p %p\n", &(((volatile uint16_t *) &nextSample)[adcIsrIndex]), &nextSample, &adcIsrIndex);
    for (int i = 0; i < 6; i++) {
        //delay(1);
        //debugPrintf("i %d %x\n", i, ((uint32_t *) &adcIsrIndex)[i]);
    }
    SPI0_SR |= SPI_SR_RFDF;
    adcIsrIndex++;
    //debugPrintf("adcIsrIndex %d\n", adcIsrIndex);
    if (adcIsrIndex < (DMA_SAMPLE_DEPTH / (16 / 8)) * DMA_SAMPLE_NUMAXES) {
        SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
    } else {
        assert(adcIsrIndex == (sizeof(adcSample) / (16 / 8)));
        if (adcIsrIndex != (sizeof(adcSample) / (16 / 8))) {
            debugPrintf("Adc isr is %d\n", adcIsrIndex);
        }
        adcSamplesRead[frontOfBuffer] = nextSample;
        frontOfBuffer = (frontOfBuffer + 1) % DMASIZE;
    }
    //debugPrintf("spiend");
    interrupts();
}

void beginAdcRead(void) {
    noInterrupts();
    if (adcIsrIndex != (sizeof(adcSample) / (16 / 8)) && adcIsrIndex != 0) {
        return;
    }
    adcIsrIndex = 0;
    SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
    interrupts();
}

void dmaReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[DMASIZE].axis1 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis2 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis3 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis4 = 0xdeadbeef;
    pinMode(18, INPUT_PULLUP);
    attachInterrupt(18, beginAdcRead, FALLING);
    SPI.begin();
    SPI0_RSER = 0x00020000;
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);
    debugPrintln("Done!");
}

/* ******** Mirror output code ********* */

void mirrorOutputSetup() {
    debugPrintln("Mirror setup starting.");
    SPI2.begin();
    SPI2_RSER = 0x00020000;
    NVIC_ENABLE_IRQ(IRQ_SPI2);
    NVIC_SET_PRIORITY(IRQ_SPI2, 0);
    debugPrintln("Done!");
}

void sendOutput(mirrorOutput& output) {
    noInterrupts();
    currentOutput = output;
    assert(mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8));
    mirrorOutputIndex = 0;
    SPI2_PUSHR = ((uint16_t) mirrorOutputIndex) | SPI_PUSHR_CTAS(1);
    interrupts();
}

void spi2_isr(void) {
    noInterrupts();
    assert(mirrorOutputIndex < sizeof(mirrorOutput) / (16 / 8));
    (void) SPI2_POPR;
    uint16_t toWrite = ((volatile uint16_t *) &currentOutput)[mirrorOutputIndex];
    SPI2_SR |= SPI_SR_RFDF;
    mirrorOutputIndex++;
    if (adcIsrIndex < sizeof(mirrorOutput) / (16 / 8)) {
        SPI0_PUSHR = ((uint16_t) toWrite) | SPI_PUSHR_CTAS(1);
    } else {
        assert(mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8));
    }
    interrupts();
}


void dmaSetup() {
    mirrorOutputSetup();
    debugPrintf("Setting up dma, offset is %d\n", dmaGetOffset());
    dmaReceiveSetup();
    debugPrintf("Dma setup complete, offset is %d. Setting up ftm timers.\n", dmaGetOffset());
    init_FTM0();
    debugPrintf("FTM timer setup complete.\n");
}
