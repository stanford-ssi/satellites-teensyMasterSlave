#include <ssiSpi.h>
#ifndef STATES_H
#define STATES_H

#define IDLE_STATE 0
#define SETUP_STATE 1
#define IMU_STATE 2
#define ANOMALY_STATE 3

#define PERFORM_IMU_COMMAND 0

extern volatile uint16_t state;

#endif
