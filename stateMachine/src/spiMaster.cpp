#include "spiMaster.h"
#include <array>

#define sizeofAdcSample (sizeof(adcSample) / 2)

const uint8_t ADC0_CS = 35;
const uint8_t ADC1_CS = 37;
const uint8_t ADC2_CS = 7;
const uint8_t ADC3_CS = 2;
const uint8_t trigger_pin = 18;  // PTB11.  This pin will receive conversion ready signal

unsigned long lastSampleTime = 0;

SPISettings spi_settings(6250000, MSBFIRST, SPI_MODE0);

uint32_t frontOfBuffer = 0;
uint32_t backOfBuffer = 0;
adcSample adcSamplesRead[ADC_READ_BUFFER_SIZE + 1];
volatile adcSample nextSample;
volatile unsigned int adcIsrIndex = 0; // indexes into nextSample
volatile unsigned int numSamplesRead = 0;

mirrorOutput currentOutput;
volatile unsigned int mirrorOutputIndex = sizeof(mirrorOutput) / (16 / 8);

//TODO:remove
volatile int numFail = 0;
volatile int numSuccess = 0;
volatile int numStartCalls = 0;
volatile int numSpi0Calls = 0;

uint32_t adcGetOffset() {
    uint32_t offset;
    if (frontOfBuffer >= backOfBuffer) {
        offset = frontOfBuffer - backOfBuffer;
    } else {
        assert(frontOfBuffer + ADC_READ_BUFFER_SIZE >= backOfBuffer);
        offset = frontOfBuffer + ADC_READ_BUFFER_SIZE - backOfBuffer;
    }
    return offset;
}

bool adcSampleReady() {
    return adcGetOffset() >= 1;
}

volatile adcSample* adcGetSample() {
    assert(adcSampleReady());
    volatile adcSample* toReturn = &adcSamplesRead[backOfBuffer];
    backOfBuffer = (backOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
    return toReturn;
}

void adcStartSampling() { // Clears out old samples so the first sample you read is fresh
    backOfBuffer = frontOfBuffer;
}

void init_FTM0(){
    analogWriteFrequency(3, 4000);
    analogWrite(3, 128);
}

/* ********* Adc read code ********* */

void checkChipSelect(void) {
    if (adcIsrIndex == 0) {
        // Take no chances
        digitalWriteFast(ADC0_CS, LOW);
        digitalWriteFast(ADC1_CS, HIGH);
        digitalWriteFast(ADC2_CS, HIGH);
        digitalWriteFast(ADC3_CS, HIGH);
    } else if (adcIsrIndex == 2) {
        digitalWriteFast(ADC0_CS, HIGH);
        digitalWriteFast(ADC1_CS, LOW);
        digitalWriteFast(ADC2_CS, HIGH);
        digitalWriteFast(ADC3_CS, HIGH);
    } else if (adcIsrIndex == 4) {
        digitalWriteFast(ADC0_CS, HIGH);
        digitalWriteFast(ADC1_CS, HIGH);
        digitalWriteFast(ADC2_CS, LOW);
        digitalWriteFast(ADC3_CS, HIGH);
    } else if (adcIsrIndex == 6) {
        digitalWriteFast(ADC0_CS, HIGH);
        digitalWriteFast(ADC1_CS, HIGH);
        digitalWriteFast(ADC2_CS, HIGH);
        digitalWriteFast(ADC3_CS, LOW);
    }
}

void spi0_isr(void) {
    uint16_t spiRead = SPI0_POPR;
    (void) spiRead;
    SPI0_SR |= SPI_SR_RFDF;
    ((volatile uint16_t *) &nextSample)[adcIsrIndex] = spiRead;
    checkChipSelect();
    numSpi0Calls++;
    adcIsrIndex++;
    if (!(adcIsrIndex < sizeofAdcSample)) {
        adcSamplesRead[frontOfBuffer] = nextSample;
        frontOfBuffer = (frontOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
        numSamplesRead += 1;
        adcIsrIndex++;
        return;
    } else {
        SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
    }
}

void beginAdcRead(void) {
    numStartCalls++;
    long timeNow = micros();

    // For debugging only; Check time since last sample
    long diff = timeNow - lastSampleTime;
    if (lastSampleTime != 0) {
        if(!(diff >= 245 && diff <= 255)) {
            debugPrintf("Diff is %d, %d success %d fail %d %d\n", diff, numSuccess, numFail, numStartCalls, numSpi0Calls);
            numFail++;
        } else {
            numSuccess++;
        }
    }
    lastSampleTime = timeNow;

    if (adcIsrIndex < (sizeof(adcSample) / (16 / 8)) && adcIsrIndex != 0) {
        debugPrintf("Yikes -- we're reading adc already\n");
        if (diff <= 245) {
            return;
        }
    }
    adcIsrIndex = 0;
    SPI0_PUSHR = ((uint16_t) adcIsrIndex) | SPI_PUSHR_CTAS(1);
}

void adcReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis1 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis2 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis3 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis4 = 0xdeadbeef;
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
    if (!assert(mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8))) {
        //return;
    }
    noInterrupts();
    currentOutput = output;
    mirrorOutputIndex = 0;
    (void) SPI2_POPR;
    SPI2_PUSHR = ((uint16_t) mirrorOutputIndex) | SPI_PUSHR_CTAS(1);
    interrupts();
}

void spi2_isr(void) {
    assert(mirrorOutputIndex < sizeof(mirrorOutput) / (16 / 8));
    (void) SPI2_POPR;
    uint16_t toWrite = ((volatile uint16_t *) &currentOutput)[mirrorOutputIndex];
    SPI2_SR |= SPI_SR_RFDF;
    mirrorOutputIndex++;
    if (mirrorOutputIndex < sizeof(mirrorOutput) / (16 / 8)) {
        SPI2_PUSHR = ((uint16_t) toWrite) | SPI_PUSHR_CTAS(1);
    } else {
        assert(mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8));
    }
}


void spiMasterSetup() {
    mirrorOutputSetup();
    debugPrintf("Setting up dma, offset is %d\n", adcGetOffset());
    adcReceiveSetup();
    debugPrintf("Dma setup complete, offset is %d. Setting up ftm timers.\n", adcGetOffset());
    init_FTM0();
    debugPrintf("FTM timer setup complete.\n");
}
