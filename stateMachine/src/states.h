#include <ssiSpi.h>
#ifndef STATES_H
#define STATES_H

#define IDLE_STATE 0
#define SETUP_STATE 1
#define IMU_STATE 2
#define TRACKING_STATE 3
#define ANOMALY_STATE 4
#define SHUTDOWN_STATE 5

// state must be between 0 and MAX_STATE
#define MAX_STATE 5

extern volatile uint16_t state;

#endif
