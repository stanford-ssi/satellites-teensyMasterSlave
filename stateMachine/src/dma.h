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
    adcSample(volatile adcSample& s) {
        copy(s);
    }
    adcSample& operator =(const volatile adcSample& s) {
        copy(s);
        return *this;
    }
} adcSample;

#define DMASIZE 10000 // in uint32s
#define DMA_SAMPLE_DEPTH 4 // bytes
#define DMA_SAMPLE_NUMAXES 4 // uint32s

uint32_t dmaGetOffset(); // return offset of size uint32
volatile adcSample* dmaGetSample();
bool dmaSampleReady();
void dmaSetup();
void dmaStartSampling();
#endif // DMA_H
