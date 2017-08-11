#ifndef SPI_MASTER_H
#define SPI_MASTER_H
#include "mirrorDriver.h"
#include "main.h"

typedef struct adcSample {
    uint32_t axis1;
    uint32_t axis2;
    uint32_t axis3;
    uint32_t axis4;
    adcSample() {
        axis1 = axis2 = axis3 = axis4 = 0;
    }

    void swap(volatile uint32_t& axis) volatile {
        uint16_t temp = axis % (1 << 16);
        axis = (axis / (1 << 16)) + (temp << 16);
    }

    // Teensy memory appears to be little-ended, so our 16-bit words are
    // in the wrong order.  This problem appears because our spiMaster
    // makes consecutive 16-bit writes
    void correctEndianness() volatile {
        swap(axis1);
        swap(axis2);
        swap(axis3);
        swap(axis4);
    }

    void copy(const volatile adcSample& s) {
        this->axis1 = s.axis1;
        this->axis2 = s.axis2;
        this->axis3 = s.axis3;
        this->axis4 = s.axis4;
    }
    adcSample(const volatile adcSample& s) {
        copy(s);
    }
    adcSample& operator =(const volatile adcSample& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "axis1 %u, axis2 %u, axis3 %u, axis4 %u", (unsigned int) axis1, (unsigned int) axis2, (unsigned int) axis3, (unsigned int) axis4);
    }

    // Sum of memory, by uint16s
    uint16_t getChecksum() volatile {
        int size = sizeof(adcSample) * 8 / 16; // uint16_t units
        uint16_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum += ((uint16_t *) this)[i];
        }
        return checksum;
    }
} adcSample;

#define ADC_READ_BUFFER_SIZE 2500 // in adcSamples

extern volatile unsigned int numSamplesRead;

uint32_t adcGetOffset();
volatile adcSample* adcGetSample();
bool adcSampleReady();
void spiMasterSetup();
void adcStartSampling();
#endif // SPI_MASTER_H
