#include "tracking.h"
#include "pidData.h"
#include "main.h"
#include "spiMaster.h"
#include "incoherent.h"
#include "pidController.h"
#include "mirrorDriver.h"
#include "states.h"

/* *** Private variables *** */

Pointer pointer;

// void sendOutput(mirrorOutput& output);

Pointer::Pointer() {
}

void Pointer::trackingSetup() {
    // Begin dma transaction
    pidDataSetup();
}

void Pointer::enterTracking() {
    enteringTracking = true; // Set this flag that we want to enter tracking mode, but the real setup work is done in firstLoopTracking()
    // This is because enterTracking() is called in an interrupt before the main loop is finished doing its thing
}

void Pointer::firstLoopTracking() {
    noInterrupts();
    numLockedOn = 0;
    samplesProcessed = 0;
    lockedOn = false;
    totalPowerReceivedBeforeIncoherent = 0;
    totalPowerReceived = 0;
    incoherentDetector.incoherentSetup();
    interrupts();
    pid.pidSetup();

    debugPrintf("About to clear dma: offset is (this might be high) %d\n", quadCell.adcGetOffset());
    quadCell.adcStartSampling();
    assert(!quadCell.adcSampleReady());
    debugPrintf("Cleared dma: offset is (this should be <= 4) %d\n", quadCell.adcGetOffset());
    assert(!enteringTracking);
    enterPidData();
    debugPrintf("Entered tracking\n");
    debugPrintf("Offset %d\n", quadCell.adcGetOffset());
    mirrorDriver.highVoltageEnable(true);
}

void Pointer::leaveTracking() {
    leavePidData();
    noInterrupts();
    enteringTracking = false;
    numLockedOn = 0;
    totalPowerReceivedBeforeIncoherent = 0;
    totalPowerReceived = 0;
    samplesProcessed = 0;
    mirrorDriver.highVoltageEnable(false);
    interrupts();
}

void Pointer::logPidSample(const volatile pidSample& s) {
    lastAdcRead.copy(s.sample);
    lastPidOut.copy(s.out);
    totalPowerReceivedBeforeIncoherent += s.sample.a;
    totalPowerReceivedBeforeIncoherent += s.sample.b;
    totalPowerReceivedBeforeIncoherent += s.sample.c;
    totalPowerReceivedBeforeIncoherent += s.sample.d;
    totalPowerReceived += s.incoherentOutput.a;
    totalPowerReceived += s.incoherentOutput.b;
    totalPowerReceived += s.incoherentOutput.c;
    totalPowerReceived += s.incoherentOutput.d;
    recordPid(s);
    samplesProcessed++;
}

void Pointer::pidProcess(const volatile adcSample& s) {
    adcSample incoherentOutput;
    incoherentDetector.incoherentProcess(s, incoherentOutput);
    lockedOn = false;
    if (lockedOn) {
        numLockedOn++;
    }

    mirrorOutput out;
    if (!changingState) {
        if (state == TRACKING_STATE) {
            double xpos = 0xbeefabcd;
            double ypos = 0xbeefdcba;
            incoherentDetector.incoherentDisplacement(incoherentOutput, xpos, ypos, theta);
            pid.pidCalculate(xpos, ypos, out);
            if (samplesProcessed % (4000 / 10) == 0) {
                mirrorDriver.sendMirrorOutput(out);
            }
        } else if (state == CALIBRATION_STATE) {
            if (samplesProcessed % (4000 / mirrorDriver.getMirrorFrequency()) == 0) {
                mirrorOutput out;
                mirrorDriver.getNextMirrorOutput(out);
                mirrorDriver.sendMirrorOutput(out);
            }
        } else {
            debugPrintf("Warning: state %d changingState %d\n", state, changingState);
        }

    } else {
        debugPrintf("Warning: changing states\n");
    }

    pidSample samplePid(s, incoherentOutput, out);
    logPidSample(samplePid);
}

void Pointer::taskTracking() {
    if (enteringTracking) {
        enteringTracking = false;
        firstLoopTracking();
    }

    if (micros() % 100 == 0) {
        assert(quadCell.adcGetOffset() < 2); // We should be able to keep up with data generation
        if (quadCell.adcGetOffset() >= 2) {
            debugPrintf("Offset is too high! It is %d\n", quadCell.adcGetOffset());
        }
    }
    if (quadCell.adcSampleReady()) {
        volatile adcSample s = *quadCell.adcGetSample();
        //s.axis1 = samplesProcessed; // TODO: remove
        pidProcess(s);
    }
    taskPidData();
}

void Pointer::trackingHeartbeat() {
    pidDataHeartbeat();
    char lastAdcReadBuf[62];
    char lastPidOutBuf[62];
    lastAdcRead.toString(lastAdcReadBuf, 60);
    lastPidOut.toString(lastPidOutBuf, 60);
    debugPrintf("Last pid output %s, last adc read %s, total power in %u (percent of max), after incoherent %u,  samples processed %u\n", lastPidOutBuf, lastAdcReadBuf, (uint32_t) (totalPowerReceivedBeforeIncoherent / maxPower / 4), (uint32_t) (totalPowerReceived / maxPower / 4), samplesProcessed);
    int a = lastAdcRead.a/4;
    int b = lastAdcRead.b/4;
    int c = lastAdcRead.c/4;
    int d = lastAdcRead.d/4;
    (void) (a + b + c + d);
    debugPrintf("X %d Y %d\n", (a + d - b - c), a + b - c - d);
}

void Pointer::enterCalibration() {
    enterTracking();
}

void Pointer::leaveCalibration() {
    leaveTracking();
}

void Pointer::taskCalibration() {
    taskTracking();
}

void Pointer::calibrationHeartbeat() {
    trackingHeartbeat();
}
