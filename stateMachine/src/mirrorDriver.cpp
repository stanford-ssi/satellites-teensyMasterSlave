/*
 * Teensy 3.2 SPI test to control Mirrorcle PicoAmp MEMS mirror driver board.
 * PicoAmp board is based on AD5664R quad 16 bit DAC with SPI input.
 * Michael Taylor
 * Aug 8th(ish) 2017
 *
 * This program generates a varying sinusoid in X and Y, initializes the SPI, initializes the DAC, sets a period
 * then plays back the sinusoid over the MEMS driver.
 */
#include "mirrorDriver.h"

MirrorDriver mirrorDriver;

MirrorDriver::MirrorDriver() {
}

void MirrorDriver::mirrorDriverSetup() {
    // Setup Pins:
    pinMode (slaveSelectPin, OUTPUT); // SPI SS pin 10
    pinMode (DRIVER_HV_EN_pin, OUTPUT); // driver board high voltage output enable pin 8
    pinMode (LASER_EN_PIN, OUTPUT); // Laser diode switching pin 7, active low.
    // write pins low
    digitalWrite(slaveSelectPin,LOW);
    digitalWrite(DRIVER_HV_EN_pin,LOW);
    analogWrite(LASER_EN_PIN,0);
    // setup SPI
    debugPrintln("Setting up SPI");
    // initialize SPI:
    SPI2.begin();
    // setup DAC
    Serial.println("Setting up DAC");
    //  send the DAC write word one byte at a time:
    // 0x280001 FULL_RESET
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x28);
    SPI2.transfer(0x00);
    SPI2.transfer(0x01);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x380001 INT_REF_EN
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x38);
    SPI2.transfer(0x00);
    SPI2.transfer(0x01);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x20000F DAC_EN_ALL
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x20);
    SPI2.transfer(0x00);
    SPI2.transfer(0x0F);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);
    // 0x300000 LDAC_EN
    digitalWrite(slaveSelectPin,LOW);
    SPI2.transfer(0x30);
    SPI2.transfer(0x00);
    SPI2.transfer(0x00);
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(1);

    debugPrintln("Filling in sine wave");
    for (int i = 0; i < SINE_BUFFER_LENGTH; i++){
        sineWave[i].x = 32765. * sin(i*phase);
        sineWave[i].y = 32765. * cos(i*phase);
    }
    debugPrintln("Filling in other buffers");
    corners[0].x = 0;
    corners[0].y = 0;
    corners[1].x = 0;
    corners[1].y = 0;
    corners[2].x = 0;
    corners[2].y = 0;
    corners[3].x = 32765;
    corners[3].y = 32765;
    corners[4].x = -32765;
    corners[4].y = -32765;
    corners[5].x = 32765;
    corners[5].y = -32765;
    corners[6].x = -32765;
    corners[6].y = 32765;
    zero[0].x = 0;
    zero[0].y = 0;
    // Start the LASER PWM timer
    // 50% duty cycle at desired frequency
    debugPrintln("Turning on laser");
    analogWriteFrequency(LASER_EN_PIN, 1000);
    analogWrite (LASER_EN_PIN,0);
}

void MirrorDriver::sendMirrorOutput(const mirrorOutput& out) {
    volatile uint32_t timeNow = millis();
    // millis() subject to overflow after 40 days, causes 1 assertion error
    assert(timeNow - timeOfLastMirrorOutput >= 1); // No more than 500ish Hz
    timeOfLastMirrorOutput = timeNow;
    int16_t x = out.x; // We truncate x to 16-bit before multiply by 0.9
    x *= 0.9; // Important: never max out the mirror
    int16_t y = out.y;
    y *= 0.9;

    // figure out the sine wave output for each channel, from -1,1 to uint16_t centered at 32768
    // X = A - B
    DAC_ch_A = (uint16_t) (32768 + (ampl_x * x / 2));
    DAC_ch_B = (uint16_t) (32768 - (ampl_x * x / 2));
    // Y = C - D
    // 90 degree phase shift on Y for a circle>?
    DAC_ch_C = (uint16_t) (32768 + (ampl_y * y / 2));
    DAC_ch_D = (uint16_t) (32768 - (ampl_y * y / 2));

  /* HERE BE STUPID */
    // Write all 4 channels 1 at a time
    DAC_write_word[0] = 0b00011000; // write to and update (C = 011) channel A DAC (A = 000), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_A >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_A & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011001; // write to and update (C = 011) channel B DAC (B = 001), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_B >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_B & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011010; // write to and update (C = 011) channel C DAC (C = 010), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_C >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_C & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
    DAC_write_word[0] = 0b00011011; // write to and update (C = 011) channel D DAC (D = 011), first 2 bits dont care.
    DAC_write_word[1] = DAC_ch_D >> 8;     // MSB
    DAC_write_word[2] = DAC_ch_D & 0x00FF; // LSB

    // manually toggle SS pin (blergh)
    digitalWrite(slaveSelectPin,LOW);
    //  send the DAC write word one byte at a time:
    SPI2.transfer(DAC_write_word[0]);
    SPI2.transfer(DAC_write_word[1]);
    SPI2.transfer(DAC_write_word[2]);
    // take the SS pin high to de-select the chip:
    digitalWrite(slaveSelectPin,HIGH);
    delayMicroseconds(10);
}

void MirrorDriver::highVoltageEnable(bool enable) {
    debugPrintf("High voltage %d\n", enable);
    if (enable) {
        digitalWrite(DRIVER_HV_EN_pin,HIGH);
    } else {
        digitalWrite(DRIVER_HV_EN_pin,LOW);
    }
}

void MirrorDriver::laserEnable(bool enable) {
    if (enable) {
        laser_state = HIGH;
        analogWrite (LASER_EN_PIN, 128); // 50% duty cycle 1khz
    } else {
        laser_state = LOW;
        analogWrite (LASER_EN_PIN, 0);
    }
}

uint16_t MirrorDriver::getMirrorFrequency() {
    return mirrorFrequency;
}

// Selection: from 0 to numMirrorBuffer - 1, selects the buffer to take cal sweeps from
// Frequency: in Hz, the frequency to send outputs
// Amplitude: from 0 to 1000, multiplies the mirror outputs by amplitude / 1000 (this is only ever used for attenuation, not gain)
void MirrorDriver::selectMirrorBuffer(uint16_t selection, uint16_t frequency, uint16_t amplitude) {
    assert(selection < numMirrorBuffers);
    assert(frequency <= 1000);
    assert(amplitude <= 1000);
    bufferSelect = selection;
    mirrorFrequency = frequency;
    currentCalibrationOutputIndex = 0;
    calibrationAmplitudeMultiplier = amplitude / 1000.0;
    if (bufferSelect == 0) {
        calibrationMirrorOutputs = sineWave;
        calibrationBufferLength = sizeof(sineWave) / sizeof(mirrorOutput);
        assert(calibrationBufferLength == SINE_BUFFER_LENGTH);
    } else if (bufferSelect == 1) {
        calibrationMirrorOutputs = corners;
        calibrationBufferLength = sizeof(corners) / sizeof(mirrorOutput);
        assert(calibrationBufferLength == 7);
    } else if (bufferSelect == 2) {
        calibrationMirrorOutputs = zero;
        calibrationBufferLength = sizeof(zero) / sizeof(mirrorOutput);
        assert(calibrationBufferLength == 1);
    } else {
        errors++;
        debugPrintf("Buffer select %d requested\n", bufferSelect);
    }
}

void MirrorDriver::getNextMirrorOutput(mirrorOutput& out) {
    out.copy(calibrationMirrorOutputs[currentCalibrationOutputIndex]);
    out.x *= calibrationAmplitudeMultiplier;
    out.y *= calibrationAmplitudeMultiplier;
    currentCalibrationOutputIndex = (currentCalibrationOutputIndex + 1) % calibrationBufferLength;
}
