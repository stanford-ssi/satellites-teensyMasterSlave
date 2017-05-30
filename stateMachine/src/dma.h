#ifndef DMA_H
#define DMA_H
#include "main.h"

#define DMASIZE 1000
extern volatile uint8_t dmaSrc[DMASIZE];

void dmaSetup();
#endif // DMA_H
