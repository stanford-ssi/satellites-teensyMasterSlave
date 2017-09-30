#ifndef INCOHERENT_H
#define INCOHERENT_H

#include "spiMaster.h"

class IncoherentDetector {
public:
    IncoherentDetector();
    void incoherentSetup();
    void incoherentProcess(const volatile adcSample& s, adcSample& output);
    void incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta);
private:
    const static int samples_per_cell = 32;
    const static int numCells = 4;
    int envelope[4] = {0,1,0,-1};
    const static int buffer_length = samples_per_cell*numCells;
    //Stores each sample for each cell to use for the incoherent detection
    volatile int32_t buff[buffer_length];
    //Keeps track of where in the rolling buffer I am
    volatile int sample = 0;
    //Stores the running sum of the previous four samples of each quad cell for both sine and cosine envelopes
    volatile int32_t rolling_detectors[2*numCells] = {0,0,0,0,0,0,0,0};
};

extern IncoherentDetector incoherentDetector;

#endif
