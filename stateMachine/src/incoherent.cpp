#include "incoherent.h"

IncoherentDetector::IncoherentDetector() {
	detectA = new IncoherentDetectorCell();
	detectB = new IncoherentDetectorCell();
	detectC = new IncoherentDetectorCell();
	detectD = new IncoherentDetectorCell();
}

void IncoherentDetector::incoherentProcess(const volatile adcSample& s, adcSample& output) {
	output.a = detectA->incoherentProcess(s.a);
	output.b = detectB->incoherentProcess(s.b);
	output.c = detectC->incoherentProcess(s.c);
	output.d = detectD->incoherentProcess(s.d);
}

void IncoherentDetector::incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta){
  //Assuming axis number maps to quadrant number
  double quadA = incoherentOutput.a;
  double quadB = incoherentOutput.b;
  double quadC = incoherentOutput.c;
  double quadD = incoherentOutput.d;
  double xposIntermediate = ((quadA + quadD) - (quadB + quadC))/(quadA + quadB + quadC + quadD);
  double yposIntermediate = ((quadA + quadB) - (quadC + quadD))/(quadA + quadB + quadC + quadD);
  xpos = cos(theta) * xposIntermediate - sin(theta) * yposIntermediate;
  ypos = sin(theta) * xposIntermediate + cos(theta) * yposIntermediate;
}