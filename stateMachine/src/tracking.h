#ifndef TRACK_H
#define TRACK_H
#include <mirrorDriver.h>

void trackingSetup();
void enterTracking();
void leaveTracking();
void taskTracking();
void trackingHeartbeat();
void enterCalibration();
void leaveCalibration();
void taskCalibration();
void calibrationHeartbeat();
#endif
