#ifndef DMA_H
#define DMA_H
#include "main.h"

#define DMASIZE 10000 // in uint32s
#define DMA_SAMPLE_DEPTH 4 // bytes
#define DMA_SAMPLE_NUMAXES 4 // uint32s

uint32_t dmaGetOffset();
volatile uint32_t* dmaGetSample();
bool dmaSampleReady();
void dmaSetup();
#endif // DMA_H
