#ifndef MIRROR_H
#define MIRROR_H

#include <main.h>

typedef struct mirrorOutput {
    int32_t x;
    int32_t y;
    uint32_t useless1; // Temporary, will remove later
    uint32_t useless2;
    mirrorOutput() {
        x = y = 0;
        useless1 = useless2 = 0xbeefdbca;
    }

    void copy(const volatile mirrorOutput& s) {
        memcpy(this, (const void *) &s, sizeof(mirrorOutput));
    }
    mirrorOutput(const volatile mirrorOutput& s) {
        copy(s);
    }
    mirrorOutput& operator =(const volatile mirrorOutput& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "x %u, y %u", (unsigned int) x, (unsigned int) y);
    }

    // Sum of memory, by uint16s
    uint16_t getChecksum() {
        int size = sizeof(mirrorOutput) * 8 / 16; // uint16_t units
        uint16_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum += ((uint16_t *) this)[i];
        }
        return checksum;
    }
} mirrorOutput;

class MirrorDriver {
public:
    MirrorDriver();

    void mirrorDriverSetup();
    void getNextMirrorOutput(mirrorOutput& out);
    void sendMirrorOutput(const mirrorOutput& out);
    void laserEnable(bool enable);
    void highVoltageEnable(bool enable);
    void selectMirrorBuffer(uint16_t selection, uint16_t frequency, uint16_t amplitude);
    uint16_t getMirrorFrequency();
    const uint16_t numMirrorBuffers = 3;

private:
    const static uint16_t SINE_BUFFER_LENGTH = 1024;
    volatile uint16_t bufferSelect = 0;
    volatile uint16_t mirrorFrequency = 100;
    volatile float calibrationAmplitudeMultiplier = 1;

    uint32_t calibrationBufferLength = SINE_BUFFER_LENGTH;
    uint32_t currentCalibrationOutputIndex = 0;
    mirrorOutput sineWave[SINE_BUFFER_LENGTH];
    mirrorOutput corners[7];
    mirrorOutput zero[1];
    mirrorOutput* calibrationMirrorOutputs = sineWave;

    uint8_t DAC_write_word[3] = {0, 0, 0}; // DAC input register is 24 bits, SPI writes 8 bits at a time. Need to queue up 3 bytes (24 bits) to send every time you write to it

    // Teensy 3.6 pinouts
    const int slaveSelectPin = 43;
    const int DRIVER_HV_EN_pin = 54;
    const int LASER_EN_PIN = 22; // toggles laser on / off for modulation (PWM pin)
    bool laser_state = LOW;
    int count = 0;
    int freq_x = 1;
    int freq_y = 1;
    float ampl_x = 1.0;
    float ampl_y = 1.0;
    int signal_x = 0;
    int signal_y = 0;
    uint16_t DAC_ch_A = 32768;
    uint16_t DAC_ch_B = 32768;
    uint16_t DAC_ch_C = 32768;
    uint16_t DAC_ch_D = 32768;
    float twopi = 2*3.14159265359; // good old pi
    float phase = twopi / SINE_BUFFER_LENGTH; // phase increment for sinusoid
    volatile uint32_t timeOfLastMirrorOutput = 0;
};

extern MirrorDriver mirrorDriver;

#endif
