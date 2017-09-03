#ifndef SPI_MASTER_H
#define SPI_MASTER_H
#include "mirrorDriver.h"
#include "main.h"

// Quad cell axes: BA
//                 CD
typedef struct adcSample {
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    adcSample() {
        a = b = c = d = 0;
    }

    void swap(volatile uint32_t &axis) volatile {
        uint16_t temp = axis % (1 << 16);
        axis = (axis / (1 << 16)) + (temp << 16);
    }

    // Adc values are inverted -- reverse it back here so that
    // negative is low and positive is high
    void negate(volatile int32_t &axis) volatile {
        // Multiply by -1 works for all ints except min_int
        if (axis == (int32_t) (1U << 31)) { // min int
            axis += 1;
        }
        axis *= -1;
    }

    // Teensy memory appears to be little-ended, so our 16-bit words are
    // in the wrong order.  This problem appears because our spiMaster
    // makes consecutive 16-bit writes
    void correctFormat() volatile {
        // Axes are read in order of chip select; we map chip selects onto lettered axes
        uint32_t CS0 = a;
        uint32_t CS1 = b;
        uint32_t CS2 = c;
        uint32_t CS3 = d;
        a = CS2;
        b = CS3;
        c = CS0;
        d = CS1;
        swap((uint32_t &) a);
        swap((uint32_t &) b);
        swap((uint32_t &) c);
        swap((uint32_t &) d);
        negate(a);
        negate(b);
        negate(c);
        negate(d);
    }

    void copy(const volatile adcSample& s) {
        this->a = s.a;
        this->b = s.b;
        this->c = s.c;
        this->d = s.d;
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
        snprintf(buf, len - 1, "axis1 %d (%d volt), axis2 %d, axis3 %d, axis4 %d", (int) a, (int) toVoltage(a), (int) b, (int) c, (int) d);
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
void resetAdc();
#endif // SPI_MASTER_H
