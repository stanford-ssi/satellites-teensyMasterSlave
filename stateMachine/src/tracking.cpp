#include <tracking.h>
#include <pidData.h>
#include <main.h>
#include <spiMaster.h>
#include <incoherent.h>

/* *** Private variables *** */

mirrorOutput lastPidOut;
adcSample lastAdcRead;
volatile unsigned samplesProcessed = 0;
volatile bool enteringTracking = false;
volatile bool lockedOn = false;
volatile uint64_t numLockedOn = 0;
volatile uint64_t totalPowerReceivedBeforeIncoherent = 0;
volatile uint64_t totalPowerReceived = 0;
const uint64_t maxPower = 1ULL << 32;
const int samples_per_cell = 32;
const int numCells = 4;
const int envelope[4] = {0,1,0,-1};
//Stores each sample for each cell to use for the incoherent detection
volatile int32_t buff[samples_per_cell*numCells];
//Keeps track of where in the rolling buffer I am
volatile int sample;
//Stores the running sum of the previous four samples of each quad cell for both sine and cosine envelopes
volatile int32_t rolling_detectors[2*numCells];

void sendOutput(mirrorOutput& output);

void trackingSetup() {
    // Begin dma transaction
    pidDataSetup();
    sample = 0;
    //Fill in initial values of the buffer
    for(int i = 0; i < buffer_length; i++){
      buff[i] = 0;
    }
    for(int i = 0; i < 2*numCells; i++){
      rolling_detectors[i] = 0;
    }
}

void enterTracking() {
    enteringTracking = true; // Set this flag that we want to enter tracking mode, but the real setup work is done in firstLoopTracking()
    // This is because enterTracking() is called in an interrupt before the main loop is finished doing its thing
}

void firstLoopTracking() {
    noInterrupts();
    numLockedOn = 0;
    samplesProcessed = 0;
    lockedOn = false;
    totalPowerReceivedBeforeIncoherent = 0;
    totalPowerReceived = 0;
    interrupts();

    debugPrintf("About to clear dma: offset is (this might be high) %d\n", adcGetOffset());
    adcStartSampling();
    assert(!adcSampleReady());
    debugPrintf("Cleared dma: offset is (this should be <= 4) %d\n", adcGetOffset());
    assert(!enteringTracking);
    enterPidData();
    debugPrintf("Entered tracking\n");
    debugPrintf("Offset %d\n", adcGetOffset());
}

void leaveTracking() {
    leavePidData();
    noInterrupts();
    enteringTracking = false;
    numLockedOn = 0;
    totalPowerReceivedBeforeIncoherent = 0;
    totalPowerReceived = 0;
    samplesProcessed = 0;
    interrupts();
}

void pidProcess(const volatile adcSample& s) {
    lastAdcRead.copy(s);
    totalPowerReceivedBeforeIncoherent += s.axis1;
    totalPowerReceivedBeforeIncoherent += s.axis2;
    totalPowerReceivedBeforeIncoherent += s.axis3;
    totalPowerReceivedBeforeIncoherent += s.axis4;

    adcSample incoherentOutput;
    incoherentProcess(s, incoherentOutput);
    lockedOn = false;
    if (lockedOn) {
        numLockedOn++;
    }
    totalPowerReceived += incoherentOutput.axis1;
    totalPowerReceived += incoherentOutput.axis2;
    totalPowerReceived += incoherentOutput.axis3;
    totalPowerReceived += incoherentOutput.axis4;

    mirrorOutput out;
    out.x_low = incoherentOutput.axis1; // TODO: REMOVE
    lastPidOut.copy(out);
    pidSample samplePid(s, incoherentOutput, out);
    recordPid(samplePid);
    sendOutput(out);
    samplesProcessed++;
}

void taskTracking() {
    if (enteringTracking) {
        enteringTracking = false;
        firstLoopTracking();
    }

    if (micros() % 100 == 0) {
        assert(adcGetOffset() < 2); // We should be able to keep up with data generation
        if (adcGetOffset() >= 2) {
            debugPrintf("Offset is too high! It is %d\n", adcGetOffset());
        }
    }
    if (adcSampleReady()) {
        volatile adcSample s = *adcGetSample();
        //s.axis1 = samplesProcessed; // TODO: remove
        pidProcess(s);
    }
    taskPidData();
}

void trackingHeartbeat() {
    pidDataHeartbeat();
    char lastAdcReadBuf[40];
    char lastPidOutBuf[40];
    lastAdcRead.toString(lastAdcReadBuf, 40);
    lastPidOut.toString(lastPidOutBuf, 40);
    debugPrintf("Last pid output %s, last adc read %s, total power in %u (percent of max), after incoherent %u,  samples processed %u\n", lastPidOutBuf, lastAdcReadBuf, (uint32_t) (totalPowerReceivedBeforeIncoherent / maxPower / 4), (uint32_t) (totalPowerReceived / maxPower / 4), samplesProcessed);
}

void enterCalibration() {
    enterTracking();
}

void leaveCalibration() {
    leaveTracking();
}

void taskCalibration() {
    taskTracking();
}

void calibrationHeartbeat() {
    trackingHeartbeat();
}
