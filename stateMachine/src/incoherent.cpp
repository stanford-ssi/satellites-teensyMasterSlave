#include "incoherent.h"
#include "spiMaster.h"
#include <limits.h>
IncoherentDetector incoherentDetector;
int maxIntRoot = sqrt(INT_MAX);

IncoherentDetector::IncoherentDetector() {
    // Real setup is in incoherentSetup, in setup() function
    for (int i = 0; i < buffer_length; i++) {
        buff[i] = 0;
    }
}

void IncoherentDetector::incoherentSetup() {
  sample = 0;
  //Fill in initial values of the buffer
  for(int i = 0; i < buffer_length; i++){
    buff[i] = 0;
  }
  for(int i = 0; i < 2*numCells; i++){
    rolling_detectors[i] = 0;
  }
}
//Checks for overflows in the sum of two squared detector values.
int32_t IncoherentDetector::safeSquare (volatile int32_t detector1, volatile int32_t detector2){
  int32_t squared = 0;
  //Checks bounds for value 1.
  if (detector1 > (maxIntRoot - 1)) detector1 = maxIntRoot;
  if (detector1 < (-maxIntRoot + 1)) detector1 = -maxIntRoot;
 
  //Checks bounds for value 2.
  if (detector2 > (maxIntRoot - 1)) detector2 = maxIntRoot;
  if (detector2 < (-maxIntRoot + 1)) detector2 = -maxIntRoot;

  squared += sq(detector1/samples_per_cell);
 
 //Checks bounds for the squared value.
  if (squared > ((~(1 << 31)) - sq(detector2/samples_per_cell))) return INT_MAX;

  squared += sq(detector2/samples_per_cell);
  return squared;
}


void IncoherentDetector::incoherentProcess(const volatile adcSample& s, adcSample& output) {
  //Retrieve latest sample values;
  int32_t latest[numCells] = {(int32_t) s.a, (int32_t) s.b, (int32_t) s.c, (int32_t) s.d};

  //Ensures the sample does not exceed the buffer index
  sample = sample % samples_per_cell;

  //Determines square wave value for our "sine" and "cosine" waves
  int sinVal = envelope[sample%4];
  int cosVal = envelope[(sample + 1)%4];

  //Replaces previous adc sample with new sample, and saves the previous sample for each quad cell
  for(int i = 0; i < numCells; i++){
    int32_t previous = buff[sample * numCells + i];
    buff[sample * numCells + i] = latest[i];

    //Updates the rolling detector sums wihtout having to recalculate the entire function (subtracts previous and adds new)
    rolling_detectors[i * 2] += sinVal*(latest[i] - previous);
    rolling_detectors[i * 2 + 1] += cosVal*(latest[i] - previous);
  }

  sample++;

  //Calculates and sets the four axis incoherent values
  for(int i = 0; i < numCells; i++){
    output.a = safeSquare(rolling_detectors[0], rolling_detectors[1]);
    output.b = safeSquare(rolling_detectors[2], rolling_detectors[3]);
    output.c = safeSquare(rolling_detectors[4], rolling_detectors[5]);
    output.d = safeSquare(rolling_detectors[6], rolling_detectors[7]);
  }
}

void IncoherentDetector::incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta){
  //Assuming axis number maps to quadrant number
  double quadA = incoherentOutput.a;
  double quadB = incoherentOutput.b;
  double quadC = incoherentOutput.c;
  double quadD = incoherentOutput.d;
  if (quadA + quadB + quadC + quadD == 0) {
    xpos = 0;
    ypos = 0;
  } else {
    double xposIntermediate = ((quadA + quadD) - (quadB + quadC))/(quadA + quadB + quadC + quadD);
    double yposIntermediate = ((quadA + quadB) - (quadC + quadD))/(quadA + quadB + quadC + quadD);
    xpos = cos(theta) * xposIntermediate - sin(theta) * yposIntermediate;
    ypos = sin(theta) * xposIntermediate + cos(theta) * yposIntermediate;
  }
}
