#ifndef DMA_H
#define DMA_H
#include "main.h"

typedef struct adcSample {
    uint32_t axis1;
    uint32_t axis2;
    uint32_t axis3;
    uint32_t axis4;
    adcSample() {
        axis1 = axis2 = axis3 = axis4 = 0;
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

/*
typedef struct expandedAdcSample {
    uint32_t axis1_msb;
    uint32_t axis1_lsb;
    uint32_t axis2_msb;
    uint32_t axis2_lsb;
    uint32_t axis3_msb;
    uint32_t axis3_lsb;
    uint32_t axis4_msb;
    uint32_t axis4_lsb;
    expandedAdcSample() {
        axis1_lsb = axis2_lsb = axis3_lsb = axis4_lsb = 0;
        axis1_msb = axis2_msb = axis3_msb = axis4_msb = 0;
    }
    static void copyAxis(uint32_t &msb, uint32_t &lsb, const volatile uint32_t &axis) {
        msb = axis >> 16;
        lsb = axis % (1 << 16);
    }
    void copy(const volatile adcSample& s) {
        axis1_msb = axis2_msb = axis3_msb = axis4_msb = 0;
        copyAxis(axis1_msb, axis1_lsb, s.axis1);
        copyAxis(axis2_msb, axis2_lsb, s.axis2);
        copyAxis(axis3_msb, axis3_lsb, s.axis3);
        copyAxis(axis4_msb, axis4_lsb, s.axis4);
    }
    expandedAdcSample(const volatile adcSample& s) {
        copy(s);
    }
    expandedAdcSample& operator =(const volatile adcSample& s) {
        copy(s);
        return *this;
    }
    void toString(char* buf, int len) {
        snprintf(buf, len - 1, "axis1 %u, axis2 %u, axis3 %u, axis4 %u", (unsigned int) axis1_lsb, (unsigned int) axis2_lsb, (unsigned int) axis3_lsb, (unsigned int) axis4_lsb);
    }

    // Sum of memory, by uint16s
    uint16_t getChecksum() volatile {
        int size = sizeof(expandedAdcSample) * 8 / 16; // uint16_t units
        uint16_t checksum = 0;
        for (int i = 0; i < size; i++) {
            checksum += ((uint16_t *) this)[i];
        }
        return checksum;
    }
} expandedAdcSample;*/

#define DMASIZE 2500 // in uint32s
#define DMA_SAMPLE_DEPTH 4 // bytes
#define DMA_SAMPLE_NUMAXES 4 // uint32s

uint32_t dmaGetOffset(); // return offset of size uint32
volatile adcSample* dmaGetSample();
bool dmaSampleReady();
void dmaSetup();
void dmaStartSampling();
#endif // DMA_H
