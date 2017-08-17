#include <spiMaster.h>


const int samples_per_cell = 32;
const int numCells = 4;
const int envelope[4] = {0,1,0,-1};
//Stores each sample for each cell to use for the incoherent detection
volatile int32_t buff[samples_per_cell*numCells];
//Keeps track of where in the rolling buffer I am
volatile int sample;
//Stores the running sum of the previous four samples of each quad cell for both sine and cosine envelopes
volatile int32_t rolling_detectors[2*numCells];

void incoherentSetup(){
  sample = 0;
  //Fill in initial values of the buffer
  for(int i = 0; i < buffer_length; i++){
    buff[i] = 0;
  }
  for(int i = 0; i < 2*numCells; i++){
    rolling_detectors[i] = 0;
  }
}

void incoherentProcess(const volatile adcSample& s, adcSample& output) {
  //Retrieve latest sample values;
  int32_t latest[numCells] = {s.axis1, s.axis2, s.axis3, s.axis4};

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
    output.axis1 = sqrt(sq(rolling_detectors[0]/samples_per_cell) + sq(rolling_detectors[1]/samples_per_cell));
    output.axis2 = sqrt(sq(rolling_detectors[2]/samples_per_cell) + sq(rolling_detectors[3]/samples_per_cell));
    output.axis3 = sqrt(sq(rolling_detectors[4]/samples_per_cell) + sq(rolling_detectors[5]/samples_per_cell));
    output.axis4 = sqrt(sq(rolling_detectors[6]/samples_per_cell) + sq(rolling_detectors[7]/samples_per_cell));
  }
}

void incoherentDisplacement(const adcSample& incoherentOutput, double& xpos, double& ypos, double theta){
  //Assuming axis number maps to quadrant number
  double quadA = incoherent_vectors.axis3;
  double quadB = incoherent_vectors.axis4;
  double quadC = incoherent_vectors.axis1;
  double quadD = incoherent_vectors.axis2;
  double xpos = ((quadA + quadD) - (quadB + quadC))/(quadA + quadB + quadC + quadD);
  double ypos = ((quadA + quadB) - (quadC + quadD))/(quadA + quadB + quadC + quadD);
  xpos = cos(theta) * xpos - sin(theta) * ypos;
  ypos = sin(theta) * xpos + cos(theta) * ypos;
}
