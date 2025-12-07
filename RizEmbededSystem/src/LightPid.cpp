#include "LightPid.h"

// Constructor to initialize PID gains
PID::PID(float Kp, float Ki, float Kd)
    : _Kp(Kp), _Ki(Ki), _Kd(Kd), _previousError(0), _integral(0) {}

// Method to compute the PID output
float PID::compute(float setpoint, float measuredValue) {
    float error = setpoint - measuredValue;
    
    // Prevent integral windup
    if (std::abs(_integral) < 1000) {
        _integral += error;
    }

    float derivative = error - _previousError;
    _previousError = error;

    return _Kp * error + _Ki * _integral + _Kd * derivative;
}

// Method to reset the integral term
void PID::resetIntegral() {
    _integral = 0;
}