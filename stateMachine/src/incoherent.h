#include "spiMaster.h"


void incoherentSetup();
void incoherentProcess(const volatile adcSample& s, adcSample& output);
void incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta);
