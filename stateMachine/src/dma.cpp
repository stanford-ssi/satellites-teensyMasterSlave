#include "dma.h"
#include <array>
//#include <DMAChannel.h>

#define sizeofAdcSample (sizeof(adcSample) / 2)

const uint8_t spi_cs_pin = 15;   // pin 15 SPI0 chip select
const uint8_t trigger_pin = 18;  // PTB11.  This pin will receive conversion ready signal

unsigned long lastSampleTime = 0;

SPISettings spi_settings(15000000, MSBFIRST, SPI_MODE0);

uint32_t frontOfBuffer = 0;
uint32_t backOfBuffer = 0;
adcSample adcSamplesRead[DMASIZE + 1];
volatile adcSample nextSample;
volatile unsigned int adcIsrIndex = 0; // indexes into nextSample
volatile unsigned int numSamplesRead = 0;

mirrorOutput currentOutput;
volatile unsigned int mirrorOutputIndex;

//TODO:remove
volatile int numFail = 0;
volatile int numSuccess = 0;
volatile int numStartCalls = 0;
volatile int numSpi0Calls = 0;

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

void init_FTM0(){
    analogWriteFrequency(3, 4000);
    analogWrite(3, 128);
}

/* ********* Adc read code ********* */

void spi0_isr(void) {
    uint16_t spiRead = SPI0_POPR;
    (void) spiRead;
    SPI0_SR |= SPI_SR_RFDF;
    assert(adcIsrIndex <= sizeofAdcSample);
    //assert(numSpi0Calls % (sizeof(adcSample) / (16 / 8)) == adcIsrIndex);
    if (numSpi0Calls % sizeofAdcSample != adcIsrIndex % (sizeof(adcSample) / (16 / 8))) {
        //debugPrintf("uh oh, spi calls %d index %d num read %d\n", numSpi0Calls, adcIsrIndex, numSamplesRead);
    }
    numSpi0Calls++;
    if (!(adcIsrIndex < sizeofAdcSample - 1)) {
        adcSamplesRead[frontOfBuffer] = nextSample;
        frontOfBuffer = (frontOfBuffer + 1) % DMASIZE;
        numSamplesRead += 1;
        adcIsrIndex++;
        return;
    }
    //debugPrintf("spibegin");

    assert(adcIsrIndex < sizeofAdcSample);
    ((volatile uint16_t *) &nextSample)[adcIsrIndex] = spiRead;
    adcIsrIndex++;
    SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
}

void beginAdcRead(void) {
    digitalWriteFast(33, HIGH);
    for (volatile int i = 0; i < 10; i++) {
    }
    digitalWriteFast(33, LOW);
    // if (numStartCalls >= 2) {
    //     return;
    // }
    numStartCalls++;
    long timeNow = micros();
    if (lastSampleTime != 0) {
        long diff = timeNow - lastSampleTime;
        if(!(diff >= 245 && diff <= 255)) {
            debugPrintf("Diff is %d, %d success %d fail %d %d\n", diff, numSuccess, numFail, numStartCalls, numSpi0Calls);
            numFail++;
        } else {
            numSuccess++;
        }
    }
    lastSampleTime = timeNow;
    if (adcIsrIndex != (sizeof(adcSample) / (16 / 8)) && adcIsrIndex != 0) {
        //debugPrintf("Uh oh, adc index %d\n", adcIsrIndex);
        if (adcIsrIndex < (sizeof(adcSample) / (16 / 8))) {
            return;
        }
    }
    adcIsrIndex = 0;
    SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
}

void dmaReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[DMASIZE].axis1 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis2 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis3 = 0xdeadbeef;
    adcSamplesRead[DMASIZE].axis4 = 0xdeadbeef;
    pinMode(33, OUTPUT); // TODO:REMOVE
    SPI.begin();
    SPI.beginTransaction(spi_settings);
    SPI0_RSER = 0x00020000;
    pinMode(18, INPUT_PULLUP);
    attachInterrupt(18, beginAdcRead, FALLING);
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);
    NVIC_SET_PRIORITY(IRQ_PORTB, 0);
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
    //mirrorOutputSetup();
    debugPrintf("Setting up dma, offset is %d\n", dmaGetOffset());
    dmaReceiveSetup();
    SPI2.begin();
    debugPrintf("Dma setup complete, offset is %d. Setting up ftm timers.\n", dmaGetOffset());
    init_FTM0();
    debugPrintf("FTM timer setup complete.\n");
}
