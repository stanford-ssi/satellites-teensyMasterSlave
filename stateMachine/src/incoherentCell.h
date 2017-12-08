#ifndef INCOHERENTCELL_H
#define INCOHERENTCELL_H

#include "spiMaster.h"

class IncoherentDetectorCell {
public:
    IncoherentDetectorCell();
    int32_t incoherentProcess(const volatile int32_t &latest);
private:
    const static int numSamples = 32;
    int envelope[4] = {0,1,0,-1};

    //Stores each sample for each cell to use for the incoherent detection
    volatile int32_t buff[numSamples];
    //Keeps track of where in the rolling buffer I am
    volatile int sample = 0;
    //Stores the running sum of the previous four samples of each quad cell for both sine and cosine envelopes
    volatile int32_t rollingSumSin;
    volatile int32_t rollingSumCos;
};

#endif