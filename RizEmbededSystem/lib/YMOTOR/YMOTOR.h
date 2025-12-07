/*
 * YMOTOR Library - ESP32 Motor Control via PCF8575 I2C Expander and TB6612FNG Driver
 *
 * This library provides a simple interface for controlling dual DC motors using:
 *   - ESP32 microcontroller
 *   - PCF8575 I2C I/O expander (16-bit)
 *   - TB6612FNG dual motor driver
 *
 * HARDWARE SETUP:
 * ===============
 *
 * 1. I2C Connection (ESP32 to PCF8575):
 *    - SDA: GPIO 19
 *    - SCL: GPIO 21
 *    - PCF8575 I2C Address: 0x20
 *
 * 2. Power Control:
 *    - GPIO 25: Power enable pin (must be HIGH for system to work)
 *
 * 3. PCF8575 to TB6612FNG Pin Mapping:
 *    Motor A (Channel A):
 *      - P12 (Pin 10) -> PWMA   (PWM control)
 *      - P11 (Pin 11) -> AIN2   (Direction control 2)
 *      - P13 (Pin 12) -> AIN1   (Direction control 1)
 *
 *    Motor B (Channel B):
 *      - P06 (Pin 6)  -> PWMB   (PWM control)
 *      - P14 (Pin 14) -> BIN1   (Direction control 1)
 *      - P15 (Pin 15) -> BIN2   (Direction control 2)
 *
 *    Standby:
 *      - P03 (Pin 5)  -> STBY   (Motor driver enable/disable)
 *
 * BASIC USAGE:
 * ============
 *
 * #include <YMOTOR.h>
 *
 * YMOTOR motors;
 *
 * void setup() {
 *   Serial.begin(921600);
 *
 *   // Initialize the motor system
 *   if (motors.begin()) {
 *     Serial.println("Motors initialized successfully");
 *   }
 * }
 *
 * void loop() {
 *   // Move Motor A forward for 2 seconds
 *   motors.setMotorA(1);    // 1 = forward
 *   delay(2000);
 *
 *   // Move Motor A backward for 2 seconds
 *   motors.setMotorA(-1);   // -1 = reverse
 *   delay(2000);
 *
 *   // Stop Motor A
 *   motors.setMotorA(0);    // 0 = stop
 *   delay(1000);
 *
 *   // Control both motors simultaneously
 *   motors.setMotorA(1);
 *   motors.setMotorB(1);
 *   delay(2000);
 *
 *   // Stop all motors
 *   motors.stopAll();
 * }
 *
 * ADVANCED USAGE:
 * ===============
 *
 * // Custom pin configuration
 * YMOTOR motors;
 * motors.begin(19, 21, 0x20, 25);  // Custom SDA, SCL, I2C address, power pin
 *
 * // Enable/disable motor driver
 * motors.enable();   // Enable motor driver (STBY = HIGH)
 * motors.disable();  // Disable motor driver (STBY = LOW)
 *
 * // Get current PCF8575 state
 * uint16_t state = motors.getState();
 * Serial.printf("PCF8575 State: 0x%04X\n", state);
 *
 * MOTOR CONTROL:
 * ==============
 * - setMotorA(direction):  Control Motor A (-1=reverse, 0=stop, 1=forward)
 * - setMotorB(direction):  Control Motor B (-1=reverse, 0=stop, 1=forward)
 * - stopAll():             Stop both motors immediately
 * - enable():              Enable motor driver (STBY pin HIGH)
 * - disable():             Disable motor driver (STBY pin LOW)
 *
 * NOTES:
 * ======
 * - Always call begin() in setup() before using motor control functions
 * - Power enable pin (GPIO 25) is automatically set HIGH during begin()
 * - Motor driver standby is automatically enabled during begin()
 * - Use disable() to save power when motors are not in use
 * - PCF8575 state is maintained internally to prevent unnecessary I2C writes
 *
 * Author: YMOTOR Library
 * Version: 1.0.0
 */

#ifndef YMOTOR_H
#define YMOTOR_H

#include <Arduino.h>
#include <Wire.h>

class YMOTOR {
public:
  // Constructor
  YMOTOR();

  // Initialization
  // Initialize with default pins (SDA=19, SCL=21, Addr=0x20, Power=25)
  bool begin();

  // Initialize with custom pins
  bool begin(uint8_t sda, uint8_t scl, uint8_t pcf_address, uint8_t power_pin);

  // Motor control methods
  // direction: -1 = reverse, 0 = stop, 1 = forward
  void setMotorA(int8_t direction);
  void setMotorB(int8_t direction);
  void stopAll();

  // Motor driver control
  void enable();   // Enable motor driver (STBY = HIGH)
  void disable();  // Disable motor driver (STBY = LOW)

  // State query
  uint16_t getState();  // Get current PCF8575 state

private:
  // Pin definitions (PCF8575 port numbers)
  static const uint8_t MOTOR_A_IN1 = 12;
  static const uint8_t MOTOR_A_IN2 = 11;
  static const uint8_t MOTOR_A_PWM = 10;

  static const uint8_t MOTOR_B_IN1 = 14;
  static const uint8_t MOTOR_B_IN2 = 15;
  static const uint8_t MOTOR_B_PWM = 6;

  static const uint8_t MOTOR_STBY = 5;

  // I2C configuration
  uint8_t _i2c_sda;
  uint8_t _i2c_scl;
  uint8_t _pcf_address;
  uint8_t _power_pin;
  uint32_t _i2c_freq;

  // PCF8575 state tracking
  uint16_t _pcf8575_state;

  // Low-level PCF8575 control
  void writePCF8575(uint16_t value);
  void setPCF8575Pin(uint8_t pin, bool value);
};

#endif // YMOTOR_H
