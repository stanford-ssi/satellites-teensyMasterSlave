#include "spiMaster.h"
#include "main.h"
#include <array>

/* *** Private Constants *** */
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
#define ADC_OVERSAMPLING_RATE 64
const unsigned int control_word = 0b1000110000010000;

void mirrorOutputSetup();
void adcReceiveSetup();
void init_FTM0();

/* *** Internal Telemetry -- SPI0 *** */
unsigned long lastSampleTime = 0;
volatile int numFail = 0;
volatile int numSuccess = 0;
volatile int numStartCalls = 0;
volatile int numSpi0Calls = 0;
volatile unsigned int numSamplesRead = 0;

/* *** SPI0 Adc Reading *** */
SPISettings adcSpiSettings(6250000, MSBFIRST, SPI_MODE0); // For SPI0
uint32_t frontOfBuffer = 0;
uint32_t backOfBuffer = 0;
adcSample adcSamplesRead[ADC_READ_BUFFER_SIZE + 1];
volatile adcSample nextSample;
volatile unsigned int adcIsrIndex = 0; // indexes into nextSample

/* *** SPI2 Mirror Driver *** */
mirrorOutput currentOutput;
volatile unsigned int mirrorOutputIndex = sizeof(mirrorOutput) / (16 / 8);

/* *** Adc Reading Public Functions *** */
uint32_t adcGetOffset() { // Returns number of samples in buffer
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
    toReturn->correctFormat();
    backOfBuffer = (backOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
    return toReturn;
}

void adcStartSampling() { // Clears out old samples so the first sample you read is fresh
    backOfBuffer = frontOfBuffer;
}

void spiMasterSetup() {
    mirrorOutputSetup();
    debugPrintf("Setting up dma, offset is %d\n", adcGetOffset());
    adcReceiveSetup();
    debugPrintf("Dma setup complete, offset is %d. Setting up ftm timers.\n", adcGetOffset());
    init_FTM0();
    debugPrintf("FTM timer setup complete.\n");
}

/* ********* Private Interrupt-based Adc Read Code ********* */

void checkChipSelect(void) {
    if (adcIsrIndex == 0) {
        // Take no chances
        digitalWriteFast(ADC_CS0, LOW);
        digitalWriteFast(ADC_CS1, HIGH);
        digitalWriteFast(ADC_CS2, HIGH);
        digitalWriteFast(ADC_CS3, HIGH);
    } else if (adcIsrIndex == 2) {
        digitalWriteFast(ADC_CS0, HIGH);
        digitalWriteFast(ADC_CS1, LOW);
        digitalWriteFast(ADC_CS2, HIGH);
        digitalWriteFast(ADC_CS3, HIGH);
    } else if (adcIsrIndex == 4) {
        digitalWriteFast(ADC_CS0, HIGH);
        digitalWriteFast(ADC_CS1, HIGH);
        digitalWriteFast(ADC_CS2, LOW);
        digitalWriteFast(ADC_CS3, HIGH);
    } else if (adcIsrIndex == 6) {
        digitalWriteFast(ADC_CS0, HIGH);
        digitalWriteFast(ADC_CS1, HIGH);
        digitalWriteFast(ADC_CS2, HIGH);
        digitalWriteFast(ADC_CS3, LOW);
    }
    volatile unsigned long long i = 0;
    for (int j = 0; j < 3; j++) {
        i += micros();
    }
    (void) i;
}

void spi0_isr(void) {
    if(adcIsrIndex >= sizeofAdcSample) {
        errors++;
    }
    uint16_t spiRead = SPI0_POPR;
    (void) spiRead; // Clear spi interrupt
    SPI0_SR |= SPI_SR_RFDF;

    ((volatile uint16_t *) &nextSample)[adcIsrIndex] = spiRead;
    numSpi0Calls++;
    adcIsrIndex++;
    checkChipSelect();
    if (!(adcIsrIndex < sizeofAdcSample)) {
        // Sample complete -- push to buffer
        adcSamplesRead[frontOfBuffer] = nextSample;
        frontOfBuffer = (frontOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
        numSamplesRead += 1;
        adcIsrIndex++;
        return;
    } else {
        SPI0_PUSHR = ((uint16_t) 0x0000) | SPI_PUSHR_CTAS(1);
    }
}

/* Entry point to an adc read.  This sends the first spi word, the rest of the work
 * is done in spi0_isr interrupts
 */
void beginAdcRead(void) {
    noInterrupts();
    numStartCalls++;
    long timeNow = micros();

    // For debugging only; Check time since last sample
    long diff = timeNow - lastSampleTime;
    if (lastSampleTime != 0) {
        lastSampleTime = timeNow;
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
            interrupts();
            return;
        }
    }

    // Cleared for takeoff
    adcIsrIndex = 0;
    checkChipSelect();
    SPI0_PUSHR = ((uint16_t) 0x0000) | SPI_PUSHR_CTAS(1);
    interrupts();
}

void setupHighVoltage() {
    pinMode(ENABLE_170_PIN, OUTPUT);
    pinMode(ENABLE_MINUS_7_PIN, OUTPUT);
    pinMode(ENABLE_7_PIN, OUTPUT);
    digitalWrite(ENABLE_170_PIN, LOW); // +170 driver enable
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
    setupAdcChipSelects();
    pinMode(sync_pin, OUTPUT);
    digitalWrite(sync_pin, LOW);
    digitalWrite(sync_pin, HIGH);
    digitalWrite(sync_pin, LOW);
    const uint8_t chipSelects[4] = {ADC_CS0, ADC_CS1, ADC_CS2, ADC_CS3};
    for (int i = 0; i < 4; i++) {
        delay(1);
        digitalWrite(chipSelects[i], LOW);
        uint16_t result = SPI.transfer16(control_word);
        (void) result;
        debugPrintf("Sent control word adc %d, received %x\n", i, result);
        digitalWrite(chipSelects[i], HIGH);
    }

    // Clear pop register - we don't want to fire the spi interrupt
    (void) SPI0_POPR; (void) SPI0_POPR;
    SPI0_SR |= SPI_SR_RFDF;
}

void init_FTM0(){
    pinMode(sample_clock, OUTPUT);
    analogWriteFrequency(sample_clock, 4000 * ADC_OVERSAMPLING_RATE);
    analogWrite(sample_clock, 5); // Low duty cycle - if we go too low it won't even turn on
}

void adcReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis1 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis2 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis3 = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].axis4 = 0xdeadbeef;
    SPI.begin();
    SPI.beginTransaction(adcSpiSettings);
    setupAdc();
    SPI0_RSER = 0x00020000; // Transmit FIFO Fill Request Enable -- Interrupt on transmit complete
    pinMode(trigger_pin, INPUT_PULLUP);
    attachInterrupt(trigger_pin, beginAdcRead, FALLING);
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);
    NVIC_SET_PRIORITY(IRQ_PORTA, 0); // Trigger_pin should be port A
    NVIC_SET_PRIORITY(IRQ_PORTE, 0); // Just in case it's E
    debugPrintln("Done!");
}

/* ******** Mirror output code ********* */

void mirrorOutputSetup() {
    debugPrintln("Mirror setup starting.");
    SPI2.begin();
    SPI2_RSER = 0x00020000; // Transmit FIFO Fill Request Enable -- Interrupt on transmit complete
    NVIC_ENABLE_IRQ(IRQ_SPI2);
    NVIC_SET_PRIORITY(IRQ_SPI2, 0);
    debugPrintln("Done!");
}

unsigned long timeOfLastOutput = 0;

/* Entry point to outputting a mirrorOutput; sending from SPI2 will trigger spi2_isr,
 * which sends the rest of the mirrorOutput
 */
void sendOutput(mirrorOutput& output) {
    long timeNow = micros();
    long diff = timeNow - timeOfLastOutput;
    assert(diff > 0);
    if (!mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8)) {
        if (diff % 100 == 0) {
            debugPrintf("spiMaster.cpp:254: mirrorIndex %d\n", mirrorOutputIndex);
        }
        bugs++;
        errors++;
        // Not done transmitting the last mirror output
        if (timeOfLastOutput != 0 && diff < 500) {
            return;
        } else {
            // The last mirror output is taking too long, reset it
        }
    }
    timeOfLastOutput = timeNow;
    noInterrupts();
    currentOutput = output;
    mirrorOutputIndex = 0;
    (void) SPI2_POPR;
    SPI2_PUSHR = ((uint16_t) mirrorOutputIndex) | SPI_PUSHR_CTAS(1);
    interrupts();
}

void spi2_isr(void) {
    if (mirrorOutputIndex >= sizeof(mirrorOutput) / (16 / 8)) {
        errors++;
    }
    (void) SPI2_POPR;
    uint16_t toWrite = ((volatile uint16_t *) &currentOutput)[mirrorOutputIndex];
    SPI2_SR |= SPI_SR_RFDF; // Clear interrupt
    mirrorOutputIndex++;
    if (mirrorOutputIndex < sizeof(mirrorOutput) / (16 / 8)) {
        SPI2_PUSHR = ((uint16_t) toWrite) | SPI_PUSHR_CTAS(1);
    } else {
        // Done sending
        assert(mirrorOutputIndex == sizeof(mirrorOutput) / (16 / 8));
    }
}
