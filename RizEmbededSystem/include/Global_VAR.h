#pragma once

#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define DEBUGGER 0
#define DEBUG_SENSOR 1

// Full PCB
#define THE_SDA 33
#define THE_SCL 22
#define RGB_LED_PIN 23
// #define buzzer_pin 18
#define buzzer_pin 4
#define BUTTON_PIN 16

#define BAUD_RATE 921600

#define TURNED_ON 1
#define TURNED_OFF 0

//rgb&light
#define RGB_ON 1
#define RGB_OFF 0
#define LED_COUNT 48
#define RGB_REST_INTENSITY 255// 0~255
#define RGB_INTENSITY 105
#define RGB_INIT_MODE  0
#define RGB_OPENING_MODE 93
#define RGB_CLOSING_MODE 94
#define RGB_TIMED_MODE 5
#define RGB_CONNECTED_MODE  6
#define RGB_OP_theme 2123
#define RGB_CP_theme 2321
//usually 1 Sec
#define RGB_DISPLAY_TIME 1000 

#define led_pin 2

#define MANUAL_MODE 1
#define RANDOM_MODE 2
#define TIMED_MODE 3
#define DOUBLE_MODE 4
#define RHYTHM_MODE 5
#define MOVEMENT_MODE 6
#define OPENING_MODE 11
#define CLOSING_MODE 12
#define TERMINATE_MODE 13
#define RESTTIMESUP_MODE 14
#define PROCESSED_MODE 99
#define CONFIG_MODE 100

//msg config
#define DEFAULT_GAMEMODE 13
#define DEFAULT_BLINKBREAK 500
#define DEFAULT_TIMEDBREAK 500
#define DEFAULT_BUFFER 500
#define DEFAULT_BUZZER 1
#define DEFAULT_BUZZERTIME 500

// TF Luna sensor config
#define  TF_LUNA_THRESHOLD 300
#define  BASELINE_MEASUREMENT_FREQUENCY 20

// TF-Luna Pin Configuration
#define TOF_RX_PIN 27
#define TOF_TX_PIN 26
// #define TOF_RX_PIN 22
// #define TOF_TX_PIN 33
#define TOF_BAUD_RATE 921600

// TF-Luna Detection Parameters
#define AMPLITUDE_THRESHOLD_FACTOR 1.09f
#define DYNAMIC_BASELINE_HISTORY_SIZE 30
#define MOVING_AVG_SIZE 3
#define AMPLITUDE_SPIKE_HISTORY_SIZE 5
#define DEBOUNCE_TIME 20
#define COOLDOWN_DURATION 400   // 200ms duration
#define CONSECUTIVE_READINGS 3
#define SENSOR_DEBOUNCE_TIME 50
#define SKIP_COUNT_THRESHOLD 5
#define AMPLITUDE_THRESHOLD 5000

//BLE config
#define DEVINFO_UUID (uint16_t)0x180a
#define DEVINFO_MANUFACTURER_UUID (uint16_t)0x2a29
#define DEVINFO_NAME_UUID (uint16_t)0x2a24
#define DEVINFO_SERIAL_UUID (uint16_t)0x2a25

#define DEVICE_NAME                   "PRO"
#define SERVICE_UUID                  "ab0828b1-198e-4351-b779-901fa0e0371e"
//three characteristic in total so far, TX and OTA are used together
#define CHARACTERISTIC_MSG_UUID       "4ac8a696-9736-4e5d-932b-e9b31405049c"
#define CHARACTERISTIC_TX_UUID        "62ec0272-3ec5-11eb-b378-0242ac130003"
#define CHARACTERISTIC_OTA_UUID       "62ec0272-3ec5-11eb-b378-0242ac130005"


//MMWave - Disabled for now
// #define RADAR_RX_PIN 14  // Connect to radar's TX
// #define RADAR_TX_PIN 21  // Connect to radar's RX
// #define BUZZER_PIN 4



extern SemaphoreHandle_t xAmplitudeMutex;
extern volatile bool objectDetected;

#endif