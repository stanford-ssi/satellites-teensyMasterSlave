#include "main.h"
#include "pidController.h"

Pid pid;

Pid::Pid() {
        _dt = 0.1;

        // set gain or iterate to find gain???
        _Kp = 0.1;
        _Ki = 0.5;
        _Kd = 0.01;

        _max = 100;
        _min = -100;
}

void Pid::pidSetup(){
}

void Pid::pidCalculate(double pv_x, double pv_y, mirrorOutput& out){

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
