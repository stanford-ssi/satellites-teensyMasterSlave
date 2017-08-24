extern double _dt;
extern double _max;
extern double _min;
extern double _Kp;
extern double _Kd;
extern double _Ki;
extern double _pre_error[];
extern double _integral[];
extern double _pv[];
extern double _sp[];

double* pidCalculate(double pv_x, double pv_y, mirrorOutput& out);
void pidSetup();
