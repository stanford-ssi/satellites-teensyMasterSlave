#include "main.h"

double _dt;
double _max;
double _min;
double _Kp;
double _Kd;
double _Ki;
double _pre_error[2];
double _integral[2];
double _pv[2];
double _sp[2];

void pidSetup(){

    _dt = 0.1;

    // set gain or iterate to find gain???
    _Kp = 0.1;
    _Ki = 0.5;
    _Kd = 0.01;

    _max = 100;
    _min = -100;

    // Changed in pid_calculate function
    _pre_error[0] = 0.0;
    _pre_error[1] = 0.0;

    _integral[0] = 0.0;
    _integral[1] = 0.0;

    // x,y estimate
    _pv[0] = 20.0;
    _pv[1] = 20.0;

    // centered position
    _sp[0] = 0.0;
    _sp[1] = 0.0;
}

void pidCalculate(double pv_x, double pv_y, mirrorOutput& out){

    // Calculate error
    double error[2] = {(_sp[0] - pv_x), (_sp[1] - pv_y)};

    // Proportional term
    double p_out[2] = {(_Kp * error[0]), (_Kp * error[1])};

    // Integral term
    _integral[0] += error[0] * _dt;
    _integral[1] += error[1] * _dt;
    double i_out[2] = {(_Ki * _integral[0]),(_Ki * _integral[1])};

    // Derivative term
    double derivative[2] = {((error[0] - _pre_error[0])/_dt),((error[1] - _pre_error[1])/_dt)};
    double d_out[2] = {(_Kd * derivative[0]),(_Kd * derivative[1])};

    // Calculate total output
    out.x = p_out[0] + i_out[0] + d_out[0];
    out.y = p_out[1] + i_out[1] + d_out[1];

    // Restrict to max/min
    if (out.x > _max)
        out.x = _max;
    else if (out.x < _min)
        out.x = _min;

    if (out.y > _max)
        out.y = _max;
    else if (out.y < _min)
        out.y = _min;

    // Save error to previous error
    _pre_error[0] = error[0];
    _pre_error[1] = error[1];
}
