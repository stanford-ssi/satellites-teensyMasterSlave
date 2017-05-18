
#include "dma.h"
#include <DmaSpi.h>

volatile uint8_t dmaSrc[DMASIZE];
volatile uint8_t dmaDest[DMASIZE];
DmaSpi::Transfer trx(nullptr, 0, nullptr);

void dmaSetup() {
    setSrc();
    clrDest(dmaDest);
    DMASPI0.begin();
    DMASPI0.start();
}

void setSrc()
{
  for (size_t i = 0; i < DMASIZE; i++)
  {
    dmaSrc[i] = i;
  }
}

void clrDest(volatile uint8_t* dest_)
{
  memset((void*)dest_, 0x00, DMASIZE);
}

void compareBuffers(const volatile uint8_t* src_, const volatile uint8_t* dest_)
{
  int n = memcmp((const void*)src_, (const void*)dest_, DMASIZE);
  assert(n == 0);
}
