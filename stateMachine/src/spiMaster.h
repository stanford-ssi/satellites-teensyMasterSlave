#ifndef SPI_MASTER_H
#define SPI_MASTER_H
#include "mirrorDriver.h"
#include "main.h"

typedef struct adcSample {
    int32_t axis1;
    int32_t axis2;
    int32_t axis3;
    int32_t axis4;
    adcSample() {
        axis1 = axis2 = axis3 = axis4 = 0;
    }

    void swap(volatile uint32_t &axis) volatile {
        uint16_t temp = axis % (1 << 16);
        axis = (axis / (1 << 16)) + (temp << 16);
    }

    // Multiply by -1 works for all ints except min_int
    void negate(volatile int32_t &axis) volatile {
        if (axis == (int32_t) (1U << 31)) { // min int
            axis += 1;
        }
        axis *= -1;
    }

    // Teensy memory appears to be little-ended, so our 16-bit words are
    // in the wrong order.  This problem appears because our spiMaster
    // makes consecutive 16-bit writes
    void correctFormat() volatile {
        swap((uint32_t &) axis1);
        swap((uint32_t &) axis2);
        swap((uint32_t &) axis3);
        swap((uint32_t &) axis4);
        negate(axis1);
        negate(axis2);
        negate(axis3);
        negate(axis4);
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
    static int toVoltage(int measurement) {
        return (int) (((measurement / 1.6 / (1 << 16) / (1 << 16) * 2 * 5 + 2.5) * 20.0));
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "axis1 %d (%d volt), axis2 %d, axis3 %d, axis4 %d", (int) axis1, (int) toVoltage(axis1), (int) axis2, (int) axis3, (int) axis4);
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
