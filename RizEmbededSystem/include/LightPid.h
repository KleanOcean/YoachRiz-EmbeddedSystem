#ifndef LIGHTPID_H
#define LIGHTPID_H

#include <cmath> // Include cmath for std::abs function

class PID {
public:
    PID(float Kp, float Ki, float Kd);

    float compute(float setpoint, float measuredValue);

    void resetIntegral();

private:
    float _Kp, _Ki, _Kd;
    float _previousError;
    float _integral;
};

#endif // LIGHTPID_H 