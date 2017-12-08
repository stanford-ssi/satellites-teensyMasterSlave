#ifndef INCOHERENT_H
#define INCOHERENT_H

#include "spiMaster.h"
#include "incoherentCell.h"

class IncoherentDetector {
public:
	IncoherentDetector();
	void incoherentProcess(const volatile adcSample& s, adcSample& output);
	void incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta);
private:
    IncoherentDetectorCell* detectA;
    IncoherentDetectorCell* detectB;
    IncoherentDetectorCell* detectC;
    IncoherentDetectorCell* detectD;
};

extern IncoherentDetector incoherentDetector;

#endif