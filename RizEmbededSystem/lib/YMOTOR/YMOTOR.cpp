/*
 * YMOTOR Library - Implementation
 *
 * ESP32 Motor Control via PCF8575 I2C Expander and TB6612FNG Driver
 */

#include "YMOTOR.h"

// Constructor
YMOTOR::YMOTOR() {
  // Default pin configuration
  _i2c_sda = 19;
  _i2c_scl = 21;
  _pcf_address = 0x20;
  _power_pin = 25;
  _i2c_freq = 100000;  // 100kHz
  _pcf8575_state = 0x0000;
}

// Initialize with default pins
bool YMOTOR::begin() {
  return begin(_i2c_sda, _i2c_scl, _pcf_address, _power_pin);
}

// Initialize with custom pins
bool YMOTOR::begin(uint8_t sda, uint8_t scl, uint8_t pcf_address, uint8_t power_pin) {
  _i2c_sda = sda;
  _i2c_scl = scl;
  _pcf_address = pcf_address;
  _power_pin = power_pin;

  // Enable power FIRST (critical for system operation)
  pinMode(_power_pin, OUTPUT);
  digitalWrite(_power_pin, HIGH);
  delay(100);  // Allow power to stabilize

  // Enable internal pull-ups on I2C pins (backup if no external pull-ups)
  // Note: External 4.7kΩ pull-ups to 3.3V are recommended for better signal quality
  pinMode(_i2c_sda, INPUT_PULLUP);
  pinMode(_i2c_scl, INPUT_PULLUP);
  delay(10);

  // Initialize I2C
  Wire.begin(_i2c_sda, _i2c_scl, _i2c_freq);
  delay(50);

  // Check if PCF8575 is present
  Wire.beginTransmission(_pcf_address);
  if (Wire.endTransmission() != 0) {
    // PCF8575 not detected
    return false;
  }

  // Initialize all pins to LOW
  _pcf8575_state = 0x0000;
  writePCF8575(_pcf8575_state);
  delay(10);

  // Enable motor driver (STBY = HIGH)
  setPCF8575Pin(MOTOR_STBY, HIGH);
  delay(10);

  return true;
}

// Set Motor A direction
// direction: -1 = reverse, 0 = stop, 1 = forward
void YMOTOR::setMotorA(int8_t direction) {
  if (direction == 0) {
    // Stop motor
    setPCF8575Pin(MOTOR_A_IN1, LOW);
    setPCF8575Pin(MOTOR_A_IN2, LOW);
    setPCF8575Pin(MOTOR_A_PWM, LOW);
  } else if (direction > 0) {
    // Forward (clockwise)
    setPCF8575Pin(MOTOR_A_IN1, HIGH);
    setPCF8575Pin(MOTOR_A_IN2, LOW);
    setPCF8575Pin(MOTOR_A_PWM, HIGH);
  } else {
    // Reverse (counter-clockwise)
    setPCF8575Pin(MOTOR_A_IN1, LOW);
    setPCF8575Pin(MOTOR_A_IN2, HIGH);
    setPCF8575Pin(MOTOR_A_PWM, HIGH);
  }
}

// Set Motor B direction
// direction: -1 = reverse, 0 = stop, 1 = forward
void YMOTOR::setMotorB(int8_t direction) {
  if (direction == 0) {
    // Stop motor
    setPCF8575Pin(MOTOR_B_IN1, LOW);
    setPCF8575Pin(MOTOR_B_IN2, LOW);
    setPCF8575Pin(MOTOR_B_PWM, LOW);
  } else if (direction > 0) {
    // Forward (clockwise)
    setPCF8575Pin(MOTOR_B_IN1, HIGH);
    setPCF8575Pin(MOTOR_B_IN2, LOW);
    setPCF8575Pin(MOTOR_B_PWM, HIGH);
  } else {
    // Reverse (counter-clockwise)
    setPCF8575Pin(MOTOR_B_IN1, LOW);
    setPCF8575Pin(MOTOR_B_IN2, HIGH);
    setPCF8575Pin(MOTOR_B_PWM, HIGH);
  }
}

// Stop all motors
void YMOTOR::stopAll() {
  setPCF8575Pin(MOTOR_A_IN1, LOW);
  setPCF8575Pin(MOTOR_A_IN2, LOW);
  setPCF8575Pin(MOTOR_A_PWM, LOW);
  setPCF8575Pin(MOTOR_B_IN1, LOW);
  setPCF8575Pin(MOTOR_B_IN2, LOW);
  setPCF8575Pin(MOTOR_B_PWM, LOW);
}

// Enable motor driver
void YMOTOR::enable() {
  setPCF8575Pin(MOTOR_STBY, HIGH);
}

// Disable motor driver
void YMOTOR::disable() {
  setPCF8575Pin(MOTOR_STBY, LOW);
}

// Get current PCF8575 state
uint16_t YMOTOR::getState() {
  return _pcf8575_state;
}

// Low-level: Write 16-bit value to PCF8575
void YMOTOR::writePCF8575(uint16_t value) {
  Wire.beginTransmission(_pcf_address);
  Wire.write(value & 0xFF);        // Low byte
  Wire.write((value >> 8) & 0xFF); // High byte
  Wire.endTransmission();
}

// Low-level: Set individual pin on PCF8575
void YMOTOR::setPCF8575Pin(uint8_t pin, bool value) {
  if (pin > 15) {
    // Invalid pin number
    return;
  }

  if (value) {
    _pcf8575_state |= (1 << pin);   // Set bit
  } else {
    _pcf8575_state &= ~(1 << pin);  // Clear bit
  }

  writePCF8575(_pcf8575_state);
}
