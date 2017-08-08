#include "spiMaster.h"
#include <array>

#define sizeofAdcSample (sizeof(adcSample) / 2)

#define ENABLE_170_PIN 54
#define ENABLE_MINUS_7_PIN 52
#define ENABLE_7_PIN 53
#define ADC_CS0 35
#define ADC_CS1 37
#define ADC_CS2 7
#define ADC_CS3 2
#define trigger_pin 26 // test point 17
#define sample_clock 29 // gpio1
#define sync_pin 3 // gpio0

unsigned int control_word = 0b1000011100100000;

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
    pinMode(sample_clock, OUTPUT);
    analogWriteFrequency(sample_clock, 4000 * 64);
    analogWrite(sample_clock, 5); // Low duty cycle - if we go too low it won't even turn on
}

/* ********* Adc read code ********* */

void checkChipSelect(void) {
    // Take no chances
    digitalWriteFast(ADC_CS0, HIGH);
    digitalWriteFast(ADC_CS1, HIGH);
    digitalWriteFast(ADC_CS2, HIGH);
    digitalWriteFast(ADC_CS3, HIGH);
    if (adcIsrIndex == 0) {
        digitalWriteFast(ADC_CS0, LOW);
    } else if (adcIsrIndex == 2) {
        digitalWriteFast(ADC_CS1, LOW);
    } else if (adcIsrIndex == 4) {
        digitalWriteFast(ADC_CS2, LOW);
    } else if (adcIsrIndex == 6) {
        digitalWriteFast(ADC_CS3, LOW);
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
        if(!(diff >= 245 && diff <= 255)) { //  4kHz -> 250 microseconds
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

void setupHighVoltage() {
    pinMode(ENABLE_170_PIN, OUTPUT);
    pinMode(ENABLE_MINUS_7_PIN, OUTPUT);
    pinMode(ENABLE_7_PIN, OUTPUT);
    digitalWrite(ENABLE_170_PIN, HIGH); // +170 driver enable
    digitalWrite(ENABLE_MINUS_7_PIN, HIGH); // -7 driver enable
    digitalWrite(ENABLE_7_PIN, HIGH); // +7 driver enable
}

void setupAdcChipSelects() {
    pinMode(ADC_CS0, OUTPUT);
    pinMode(ADC_CS1, OUTPUT);
    pinMode(ADC_CS2, OUTPUT);
    pinMode(ADC_CS3, OUTPUT);
    digitalWriteFast(ADC_CS0, HIGH);
    digitalWriteFast(ADC_CS1, HIGH);
    digitalWriteFast(ADC_CS2, HIGH);
    digitalWriteFast(ADC_CS3, HIGH);
}

void setupAdc() {
    setupHighVoltage();
    pinMode(sync_pin, OUTPUT);
    digitalWrite(sync_pin, LOW);
    digitalWrite(sync_pin, HIGH);
    digitalWrite(sync_pin, LOW);
    delay(1);
    digitalWrite(ADC_CS0, LOW);
    uint16_t result = SPI.transfer16(control_word);
    (void) result;
    debugPrintf("Sent control word adc 0, received %x\n", result);
    digitalWrite(ADC_CS0, HIGH);
    delay(1);
    digitalWrite(ADC_CS1, LOW);
    result = SPI.transfer16(control_word);
    (void) result;
    debugPrintf("Sent control word adc 1, received %x\n", result);
    digitalWrite(ADC_CS1, HIGH);
    delay(1);
    digitalWrite(ADC_CS2, LOW);
    result = SPI.transfer16(control_word);
    (void) result;
    debugPrintf("Sent control word adc 2, received %x\n", result);
    digitalWrite(ADC_CS2, HIGH);
    delay(1);
    digitalWrite(ADC_CS3, LOW);
    result = SPI.transfer16(control_word);
    (void) result;
    debugPrintf("Sent control word adc 3, received %x\n", result);
    digitalWrite(ADC_CS3, HIGH);

    // Clear pop register - we don't want to fire the spi interrupt
    (void) SPI0_POPR; (void) SPI0_POPR;
    SPI0_SR |= SPI_SR_RFDF;
}

void adcReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis1 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis2 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis3 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis4 = 0xdeadbeef;
    SPI.begin();
    SPI.beginTransaction(spi_settings);
    setupAdc();
    SPI0_RSER = 0x00020000;
    pinMode(trigger_pin, INPUT_PULLUP);
    attachInterrupt(trigger_pin, beginAdcRead, FALLING);
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);
    NVIC_SET_PRIORITY(IRQ_PORTA, 0);
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
