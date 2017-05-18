#ifndef DMA_H
#define DMA_H
#include "main.h"
#include <DmaSpi.h>
// Code taken from https://github.com/crteensy/DmaSpi/blob/master/examples/DMASpi_example1/DMASpi_example1.ino
// See lib/DmaSpi/LICENSE

#define DMASIZE 1000
extern volatile uint8_t dmaSrc[DMASIZE];
extern volatile uint8_t dmaDest[DMASIZE];
//extern volatile uint8_t dmaDest1[DMASIZE];
extern DmaSpi::Transfer trx;

void dmaSetup();
void setSrc();
void clrDest(volatile uint8_t* dest_);
void compareBuffers(volatile const uint8_t* src_, const volatile uint8_t* dest_);
#endif // DMA_H
