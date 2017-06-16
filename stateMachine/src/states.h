#include <ssiSpi.h>
#ifndef STATES_H
#define STATES_H

#define IDLE_STATE 0
#define SETUP_STATE 1
#define CALIBRATION_STATE 3
#define TRACKING_STATE 4
#define ANOMALY_STATE 5
#define SHUTDOWN_STATE 6

// state must be between 0 and MAX_STATE
#define MAX_STATE 6

extern volatile uint16_t state;

#endif
