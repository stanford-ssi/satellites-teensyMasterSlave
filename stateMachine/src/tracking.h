#ifndef TRACK_H
#define TRACK_H
#include <mirrorDriver.h>
#include <spiMaster.h>
#include <pidData.h>

class Pointer {
public:
    Pointer();
    void trackingSetup();
    void enterTracking();
    void leaveTracking();
    void taskTracking();
    void trackingHeartbeat();
    void enterCalibration();
    void leaveCalibration();
    void taskCalibration();
    void calibrationHeartbeat();

    volatile uint64_t numLockedOn = 0;
    volatile unsigned samplesProcessed = 0;
    volatile uint64_t totalPowerReceivedBeforeIncoherent = 0;
    volatile uint64_t totalPowerReceived = 0;
    volatile double theta = 0;
private:
    void firstLoopTracking();
    void logPidSample(const volatile pidSample& s);
    void pidProcess(const volatile adcSample& s);

    mirrorOutput lastPidOut;
    adcSample lastAdcRead;
    volatile bool enteringTracking = false;
    volatile bool lockedOn = false;
    const uint64_t maxPower = 1ULL << 32;
};

extern Pointer pointer;


#endif
