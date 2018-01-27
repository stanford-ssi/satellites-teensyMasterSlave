#include "incoherent.h"
#include "spiMaster.h"

IncoherentDetector incoherentDetector;

IncoherentDetector::IncoherentDetector() {
    // Real setup is in incoherentSetup, in setup() function
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

//Fiona's Notes:
//adcSample: 4 unsigned ints.  We want to find 1khz input 
//Multiply row by cos, then sin, then use this equation: (cos*data)^2 + (sin*data)^2,  then sqrt
//The longer the window (64 or 128) the longer the window, the tighter the filter needs to be
//Valid Sample Example:  [-5 0 5 0]  x [1 0 -1 0] = -5 - 5 = -10  --cos function
//Invalid Sample Example:  [-5 5 -5 5] x [1 0 -1 0] = -5 + 5 = 0 --cos function
void IncoherentDetector::incoherentProcess(const volatile adcSample& s, adcSample& output) {
  //Retrieve latest sample values;
  int32_t latest[numCells] = {(int32_t) s.a, (int32_t) s.b, (int32_t) s.c, (int32_t) s.d};

  //Ensures the sample does not exceed the buffer index
  sample = sample % samples_per_cell;

  //Determines square wave value for our "sine" and "cosine" waves
  int sinVal = envelope[sample%4];
  int cosVal = envelope[(sample + 1)%4];

  /*
  //Replaces previous adc sample with new sample, and saves the previous sample for each quad cell
  for(int i = 0; i < numCells; i++){
    int32_t previous = buff[sample * numCells + i];
    buff[sample * numCells + i] = latest[i];

    //Updates the rolling detector sums wihtout having to recalculate the entire function (subtracts previous and adds new)
    rolling_detectors[i * 2] += sinVal*(latest[i] - previous);
    rolling_detectors[i * 2 + 1] += cosVal*(latest[i] - previous);
  }*/

  for (int i = 0; i < numCells; i++) {  
    //Add sin value to rolling_detectors
    rolling_detectors[(i * 2)] += IncoherentDetector::incoherentOneSinChannel(latest[i]); 
    //Add cos value to rolling_detectors
    rolling_detectors[(i * 2) + 1] += IncoherentDetector::incoherentOneCosChannel(latest[i]);
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

int32_t IncoherentDetector::incoherentOneSinChannel(int32_t current_channel) {
  //Multiply envelope value by current data value
  int32_t sinAdjustedVal = envelope[sample % 4] * current_channel;
  int32_t four_value_sin_sum = 0;
  //Loop through and add adjusted value to end, while shifting all other numbers to left
  for (int i = 0; i < (numCells - 1); i++) {
      rolling_sin_vals[i] = rolling_sin_vals[i + 1];
      four_value_sin_sum += rolling_sin_vals[i];
  }
  rolling_sin_vals[numCells - 1] = sinAdjustedVal;
  four_value_sin_sum += rolling_sin_vals[numCells - 1];
  
  //Return the sum of sin*value for the current 4-vaule window
  return four_value_sin_sum;
}

int32_t IncoherentDetector::incoherentOneCosChannel(int32_t current_channel) {
  //Multiply envelope (adjusted for cos) value by current data value
  int32_t cosAdjustedVal = envelope[(sample + 1) % 4] * current_channel;
  int32_t four_value_cos_sum = 0;
  //Loop through and add adjusted value to end, while shifting all other numbers to left
  //Adjust return value to be equal 
  for (int i = 0; i < (numCells - 1); i++) {
      rolling_cos_vals[i] = rolling_cos_vals[i + 1];
      four_value_cos_sum += rolling_cos_vals[i];
  }
  rolling_cos_vals[numCells - 1] = cosAdjustedVal;
  four_value_cos_sum += rolling_cos_vals[numCells - 1];

  //Return the sum of cos*value for the current 4-vaule window
  return four_value_cos_sum;
}
