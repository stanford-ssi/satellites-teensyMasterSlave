#include "incoherent.h"
#include "spiMaster.h"

IncoherentDetector incoherentDetector;

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
    output.a = sqrt(sq(rolling_detectors[0]/samples_per_cell) + sq(rolling_detectors[1]/samples_per_cell));
    output.b = sqrt(sq(rolling_detectors[2]/samples_per_cell) + sq(rolling_detectors[3]/samples_per_cell));
    output.c = sqrt(sq(rolling_detectors[4]/samples_per_cell) + sq(rolling_detectors[5]/samples_per_cell));
    output.d = sqrt(sq(rolling_detectors[6]/samples_per_cell) + sq(rolling_detectors[7]/samples_per_cell));
  }
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
