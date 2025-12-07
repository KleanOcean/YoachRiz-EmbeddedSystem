#ifndef TF_LUNA_UART_H
#define TF_LUNA_UART_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Global_VAR.h"
#include "Log.h"  // Add this for logging system

// Type definitions for cleaner code
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint32_t u32;

/**
 * TF_Luna_UART Class
 * 
 * This class handles communication with TF-Luna LiDAR sensor over UART.
 * Frame format: [0x59][0x59][Dist_L][Dist_H][Amp_L][Amp_H][Temp_L][Temp_H][Checksum]
 * 
 * Usage:
 * 1. Create instance with HardwareSerial and pins
 * 2. Call begin() to initialize
 * 3. Use updateLidarData() to get readings
 * 4. Use objectDetected() for presence detection
 */
class TF_Luna_UART {
private:
    HardwareSerial* _serial;       // UART interface pointer
    int _rx_pin;                   // RX pin number
    int _tx_pin;                   // TX pin number
    // Keep only the calculated threshold
    uint16_t amplitude_threshold;     // Absolute threshold value
    
    // Circular buffers for data smoothing
    u16 distanceBuffer[MOVING_AVG_SIZE];
    u16 amplitudeBuffer[MOVING_AVG_SIZE];
    uint8_t bufferIndex = 0;
    
    // Object detection state tracking
    bool lastDetectionState = false;
    unsigned long lastDetectionTime = 0;
    
    // Amplitude spike detection buffer
    uint32_t runningSum = 0;
    int oldestIndex = 0;
    bool historyFilled = false;
    int historyIndex = 0;
    uint16_t amplitudeHistory[DYNAMIC_BASELINE_HISTORY_SIZE] = {0};
    
    // Frame parsing variables
    uint8_t frame_data[9];
    uint8_t data_index = 0;
    float lastInstantPercent = 0;
    
    // Private helper methods
    bool isAmplitudeSpike(u16 currentAmp);    // Detect sudden amplitude changes
    u16 getMovingAverage(u16* buffer, uint8_t size); // Calculate moving average
    void updateBuffers(u16 distance, u16 amplitude);  // Update circular buffers
    bool parseFrame(uint8_t* frame);          // Parse 9-byte data frame
    void resetUpdateLidarData();
    bool isRunning = false;  // Add control flag
    SemaphoreHandle_t xLidarMutex;  // Add mutex for Lidar data

    // Add these members
    bool _objectDetected = false;
    unsigned long _lastDetectionTime = 0;
    uint16_t detectedAmplitude = 0;
    unsigned long detectionTimestamp = 0;

    // Add timestamp tracking
    unsigned long lastUpdateTimestamp = 0;
    unsigned long cooldownStart = 0;
    unsigned long cooldownDuration = COOLDOWN_DURATION;  // Default, can be overridden

    // Add to class members
    float maxPositivePercent = -100.0f;  // Start with minimum possible percentage
    float maxNegativePercent = 100.0f;   // Start with maximum possible percentage
    float instantPercent = 0.0f;
    int framesProcessed = 0;

public:
    // Data structure for sensor readings
    typedef struct {
        u16 u16Distance;           // Distance in cm
        u16 u16Amp;               // Signal amplitude
        int16_t temperature;      // Temperature in degrees C
        bool frame_complete;      // Frame received completely
    } TF_Luna_Data;

    TF_Luna_Data Lidar = {0, 0, 0, false};    
    int baseline_amplitude = 1000;    // Reference amplitude for detection
    float amplitude_threshold_factor = AMPLITUDE_THRESHOLD_FACTOR;  // Threshold factor for detection

    /**
     * Constructor
     * @param serial: HardwareSerial instance (e.g., Serial1, Serial2)
     * @param rx_pin: UART RX pin number
     * @param tx_pin: UART TX pin number
     */
    TF_Luna_UART(HardwareSerial* serial, int rx_pin, int tx_pin);  // Declaration only
    ~TF_Luna_UART() = default;
    
    // Prevent copying and moving
    TF_Luna_UART(const TF_Luna_UART&) = delete;
    TF_Luna_UART& operator=(const TF_Luna_UART&) = delete;
    TF_Luna_UART(TF_Luna_UART&&) = delete;
    TF_Luna_UART& operator=(TF_Luna_UART&&) = delete;

    // Public interface methods
    bool begin(uint32_t baudRate = 115200);   // Initialize sensor
    bool tryConnect();                        // Attempt connection at different baud rates
    bool configure();                         // Configure sensor settings
    uint16_t updateLidarData();              // Update sensor readings
    void printLidarData();                   // Print current readings
    void takeBaseline(bool stop_reading = true);                     // Calibrate baseline amplitude
    bool objectDetected();                   // Check for object presence
    bool checkLidarDetection();              // Process detection logic
    uint16_t computeDynamicBaseline();
    void syncFrames();
    
    // Getter methods
    uint16_t getDistance() { return Lidar.u16Distance; }
    uint16_t getAmplitude() { 
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        uint16_t amp = Lidar.u16Amp;
        xSemaphoreGive(xLidarMutex);
        return amp;
    }
    int16_t getTemperature() { return Lidar.temperature; }

    // Add these missing methods
    bool getData() {
        return updateLidarData() > 0;
    }
    
    void enableChecksum(bool enable) {
        // Implementation for checksum enable
        uint8_t cmd[] = {0x5A, 0x05, 0x08, static_cast<uint8_t>(enable ? 0x01 : 0x00), 0x00};
        _serial->write(cmd, sizeof(cmd));
    }
    
    void saveSettings() {
        // Implementation for save settings
        uint8_t cmd[] = {0x5A, 0x04, 0x11, 0x6F};
        _serial->write(cmd, sizeof(cmd));
    }

    // Add control methods
    void startReading() { isRunning = true; }
    void stopReading() { isRunning = false; }
    bool isReading() { return isRunning; }

    // Add getter for amplitude threshold
    uint16_t getAmplitudeThreshold() { return amplitude_threshold; }

    // Add these methods
    bool isObjectDetected() { 
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        bool detected = _objectDetected;
        xSemaphoreGive(xLidarMutex);
        return detected;
    }
    
    void resetDetection() {
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        _objectDetected = false;
        xSemaphoreGive(xLidarMutex);
    }

    // Add these methods
    uint16_t getDetectedAmplitude() { 
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        uint16_t amp = detectedAmplitude;
        xSemaphoreGive(xLidarMutex);
        return amp;
    }
    
    unsigned long getDetectionTimestamp() {
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        unsigned long ts = detectionTimestamp;
        xSemaphoreGive(xLidarMutex);
        return ts;
    }
    
    void clearDetectionData() {
        xSemaphoreTake(xLidarMutex, portMAX_DELAY);
        detectedAmplitude = 0;
        detectionTimestamp = 0;
        xSemaphoreGive(xLidarMutex);
    }

    // Add these methods
    unsigned long getLastUpdateTimestamp() { return lastUpdateTimestamp; }
    bool isCooldownActive() { return (millis() - cooldownStart) < cooldownDuration; }
    void resetCooldown() { cooldownStart = millis(); }
    void setCooldownDuration(unsigned long duration) { cooldownDuration = duration; }
    
    // Add this to the public section of the TF_Luna_UART class
    bool init();  // Initialize the sensor with default settings

};

#endif

