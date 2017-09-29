#include "spiMaster.h"
#include "main.h"
#include <array>
#include "mirrorDriver.h"

SPISettings adcSpiSettings(6250000, MSBFIRST, SPI_MODE0); // For SPI0
QuadCell quadCell;

QuadCell::QuadCell() {
}

/* *** Adc Reading Public Functions *** */
uint32_t QuadCell::adcGetOffset() { // Returns number of samples in buffer
    uint32_t offset;
    if (frontOfBuffer >= backOfBuffer) {
        offset = frontOfBuffer - backOfBuffer;
    } else {
        assert(frontOfBuffer + ADC_READ_BUFFER_SIZE >= backOfBuffer);
        offset = frontOfBuffer + ADC_READ_BUFFER_SIZE - backOfBuffer;
    }
    return offset;
}

bool QuadCell::adcSampleReady() {
    return adcGetOffset() >= 1;
}

int succ__ = 0;
int fail__ = 0;
volatile adcSample* QuadCell::adcGetSample() {
    assert(adcSampleReady());
    volatile adcSample* toReturn = &adcSamplesRead[backOfBuffer];
    toReturn->correctFormat();
    if (internalInterruptAdcReading && DEBUG) {
        // FOR DEBUG PURPOSES ONLY: give a deterministic value to validate spi slave comms
        toReturn->a = numSamplesRead;
    }
    backOfBuffer = (backOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
    return toReturn;
}

void QuadCell::adcStartSampling() { // Clears out old samples so the first sample you read is fresh
    backOfBuffer = frontOfBuffer;
}

void QuadCell::spiMasterSetup() {
    mirrorDriver.mirrorDriverSetup();
    debugPrintf("Setting up dma, offset is %d\n", adcGetOffset());
    adcReceiveSetup();
    debugPrintf("Dma setup complete, offset is %d. Setting up ftm timers.\n", adcGetOffset());
    debugPrintf("FTM timer setup complete.\n");
}

/* ********* Private Interrupt-based Adc Read Code ********* */

void QuadCell::checkChipSelect(void) {
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
}

void spi0_isr(void) {
    quadCell.quadCellSpi0_isr();
}

void QuadCell::quadCellSpi0_isr(void) {
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
        digitalWriteFast(ADC_CS3, HIGH);
        // Sample complete -- push to buffer
        adcSamplesRead[frontOfBuffer] = nextSample;
        frontOfBuffer = (frontOfBuffer + 1) % ADC_READ_BUFFER_SIZE;
        numSamplesRead += 1;
        adcIsrIndex++;
        /*if (micros() % 1000 == 0) {
            debugPrintf("%d\n", micros() - lastSampleTime);
        }*/
        return;
    } else {
        SPI0_PUSHR = ((uint16_t) 0x0000) | SPI_PUSHR_CTAS(1);
    }
}

/* Entry point to an adc read.  This sends the first spi word, the rest of the work
 * is done in spi0_isr interrupts
 */
void QuadCell::beginAdcRead(void) {
    noInterrupts();
    samples_taken_since_reset++;
    numStartCalls++;
    long timeNow = micros();

    // For debugging only; Check time since last sample
    long diff = timeNow - lastSampleTime;
    if (lastSampleTime != 0) {
        lastSampleTime = timeNow;
        if(!(diff >= 245 && diff <= 255)) { //  4kHz -> 250 microseconds
            // debugPrintf("Diff is %d, %d success %d fail %d %d\n", diff, numSuccess, numFail, numStartCalls, numSpi0Calls); // A blocking print inside an interrupt can cascade errors I think
            numFail++;
        } else {
            numSuccess++;
        }
    }
    lastSampleTime = timeNow;

    if (samples_taken_since_reset == 20000) {
        debugPrintf("Time for 20000 samples %d\n", timeNow - time_of_last_reset);
        time_of_last_reset = timeNow;
        samples_taken_since_reset = 0;
    }

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

void readAdcTimerIsr() {
    quadCell.quadCellReadAdcTimerIsr();
}

void readAdcEdgeIsr() {
    quadCell.quadCellReadAdcEdgeIsr();
}

void QuadCell::quadCellReadAdcEdgeIsr() {
    if (!internalInterruptAdcReading) {
        beginAdcRead();
    }
}

void QuadCell::quadCellReadAdcTimerIsr() {
    if (internalInterruptAdcReading) {
        beginAdcRead();
    }
}

void QuadCell::setupHighVoltage() {
    pinMode(ENABLE_MINUS_7_PIN, OUTPUT);
    pinMode(ENABLE_7_PIN, OUTPUT);
    digitalWrite(ENABLE_MINUS_7_PIN, HIGH); // -7 driver enable
    digitalWrite(ENABLE_7_PIN, HIGH); // +7 driver enable
}

void QuadCell::setupAdcChipSelects() {
    pinMode(ADC_CS0, OUTPUT);
    pinMode(ADC_CS1, OUTPUT);
    pinMode(ADC_CS2, OUTPUT);
    pinMode(ADC_CS3, OUTPUT);
    digitalWriteFast(ADC_CS0, HIGH);
    digitalWriteFast(ADC_CS1, HIGH);
    digitalWriteFast(ADC_CS2, HIGH);
    digitalWriteFast(ADC_CS3, HIGH);
}

void QuadCell::resetAdc() {
    analogWrite(sample_clock, 0);
    digitalWrite(sync_pin, LOW);
    delayMicroseconds(10);
    digitalWrite(sync_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(sync_pin, LOW);
    digitalWriteFast(ADC_CS0, LOW);
    digitalWriteFast(ADC_CS1, LOW);
    digitalWriteFast(ADC_CS2, LOW);
    digitalWrite(ADC_CS3, LOW);
    delayMicroseconds(5);
    uint16_t result = SPI.transfer16(control_word);
    (void) result;
    debugPrintf("Sent control word, received %x\n", result);
    delayMicroseconds(5);
    digitalWriteFast(ADC_CS0, HIGH);
    digitalWriteFast(ADC_CS1, HIGH);
    digitalWriteFast(ADC_CS2, HIGH);
    digitalWriteFast(ADC_CS3, HIGH);
    analogWrite(sample_clock, 5);
    noInterrupts();
    time_of_last_reset = micros();
    samples_taken_since_reset = 0;
    interrupts();
}

void QuadCell::setupAdc() {
    setupHighVoltage();
    setupAdcChipSelects();
    pinMode(sync_pin, OUTPUT);
    digitalWriteFast(sync_pin, LOW);
    delayMicroseconds(1);
    init_FTM0();
    // Important: for some reason the adc needs to wind up before programming with control word
    delayMicroseconds(10000);
    resetAdc();

    // Clear pop register - we don't want to fire the spi interrupt
    (void) SPI0_POPR; (void) SPI0_POPR;
    SPI0_SR |= SPI_SR_RFDF;
}

void QuadCell::init_FTM0(){
    pinMode(sample_clock, OUTPUT);
    analogWriteFrequency(sample_clock, 4000 * ADC_OVERSAMPLING_RATE);
    analogWrite(sample_clock, 5); // Low duty cycle - if we go too low it won't even turn on
}

void QuadCell::adcReceiveSetup() {
    debugPrintln("Starting.");
    adcSamplesRead[ADC_READ_BUFFER_SIZE].a = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].b = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].c = 0xdeadbeef;
    adcSamplesRead[ADC_READ_BUFFER_SIZE].d = 0xdeadbeef;
    SPI.begin();
    setupAdc();
    SPI.beginTransaction(adcSpiSettings);
    SPI0_RSER = 0x00020000; // Transmit FIFO Fill Request Enable -- Interrupt on transmit complete
    pinMode(trigger_pin, INPUT_PULLUP);
    attachInterrupt(trigger_pin, readAdcEdgeIsr, FALLING);
    NVIC_ENABLE_IRQ(IRQ_SPI0);
    NVIC_SET_PRIORITY(IRQ_SPI0, 0);
    NVIC_SET_PRIORITY(IRQ_PORTA, 0); // Trigger_pin should be port A
    NVIC_SET_PRIORITY(IRQ_PORTE, 0); // Just in case it's E
    readAdcTimer.begin(readAdcTimerIsr, 250);
    debugPrintln("Done!");
}

void QuadCell::quadCellHeartBeat() {
    debugPrintf("dma offset %d %d %d", quadCell.adcGetOffset(), backOfBuffer, frontOfBuffer);
    debugPrintf("DMA Fail %d success %d starts %d spi0s %d\n", numFail, numSuccess, numStartCalls, quadCell.numSpi0Calls);
}
