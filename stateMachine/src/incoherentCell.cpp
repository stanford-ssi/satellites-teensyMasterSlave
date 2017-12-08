#include "incoherentCell.h"

IncoherentDetectorCell::IncoherentDetectorCell() {
	//Fill in initial values of the buffer
	for(int i = 0; i < numSamples; i++){
		buff[i] = 0;
	}

	sample = 0;

	rollingSumCos = 0;
	rollingSumSin = 0;
}

int32_t IncoherentDetectorCell::incoherentProcess(const volatile int32_t &latest) {
	//Determines square wave value for our "sine" and "cosine" waves
	int sinVal = envelope[sample % 4];
 	int cosVal = envelope[(sample + 1) % 4];

 	//Save the value that is about to be overwritten
	int32_t previous = buff[sample];
	buff[sample] = latest;

	//We are replacing the overwritten previous value and no longer
	//considering it so it should be removed from the rolling sum and replaced
	//with the new latest value
	rollingSumSin = sinVal * (rollingSumSin + latest - previous);
	rollingSumCos = cosVal * (rollingSumCos + latest - previous);

	//Move to the next slot in the buffer (and wrap around if the end of the buffer
	//has been reached)
	sample = (sample + 1) % numSamples;

	return sqrt(sq(rollingSumSin / numSamples) + sq(rollingSumCos / numSamples));
}
