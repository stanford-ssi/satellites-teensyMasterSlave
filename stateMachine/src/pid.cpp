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
    double p_out[2];
    double i_out[2];
    double derivative[2];
    double d_out[2];

    for (int i = 0; i < 2; i++){
      // Proportional term
      p_out[i] = _Kp * error[i];

      // Integral term
      _integral[i] += error[i] * _dt;
      i_out[i] = _Ki * _integral[i];

      // Derivative term
      derivative[i] = (error[i] - _pre_error[i])/_dt;
      d_out[i] = _Kd * derivative[i];

      // Calculate total output
      double output = p_out[i] + i_out[i] + d_out[i];

      // Restrict to max/min
      if (output > _max)
          output = _max;
      else if (output < _min)
          output = _min;

      if (i == 0)
        out.x = output;
      else
        out.y = output;

      // Save error to previous error
      _pre_error[i] = error[i];
    }
}
