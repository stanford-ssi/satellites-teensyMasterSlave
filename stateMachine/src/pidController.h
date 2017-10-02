#ifndef PID_H
#define PID_H
#include "mirrorDriver.h"

class Pid {
public:
    volatile double _dt;
    volatile double _max;
    volatile double _min;
    volatile double _Kp;
    volatile double _Kd;
    volatile double _Ki;
    double _pre_error[2] = {0,0};
    double _integral[2] = {0,0};
    double _sp[2] = {0,0}; // centered position

    Pid();
    void pidCalculate(double pv_x, double pv_y, mirrorOutput& out);
    void pidSetup();
};

extern Pid pid;
#endif
