#ifndef MMWAVE_H
#define MMWAVE_H

#include <Arduino.h>
#include "Log.h"  // Add logging system header

class MMWave {
public:
    MMWave(uint8_t rx_pin, uint8_t tx_pin);
    bool init();  
    void begin();
    void startDetection();
    void stopDetection();
    inline bool isRunning() const { return _isRunning; }
    inline bool isTargetDetected() const { return _targetDetected; }
    inline float getTargetDistance() const { return _targetDistance; }
    inline int getSignalStrength() const { return _signalStrength; }
    bool objectDetected();
    void setExpectedSignalStrength(int val);
    void setExpectedDistance(int val);
    void setExpectedDelay(int val);
    int getExpectedSignalStrength();
    int getExpectedDistance();
    int getExpectedDelay();
    bool checkConnections();
    int getSignalStrength();
    
    // Public getter for expected signal strength
    uint16_t getExpectedSignalStrength() const;

    // Public function to check if the MMWave is started
    bool isStarted() const;

    // Add to public section
    bool isObjectInRange();

private:
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    HardwareSerial _serial;
    bool _targetDetected;
    uint16_t _targetDistance;  // Changed from int to uint16_t
    uint16_t _signalStrength;  // Changed from int to uint16_t

    uint16_t _expectedDistance;    // Changed from int to uint16_t
    uint16_t _expectedSignalStrength;  // Changed from int to uint16_t
    uint16_t _expectedDelay;  

    TaskHandle_t _detectionTaskHandle;
    volatile bool _objectInRange;
    volatile bool _isRunning;
    void update();

    // Declare detectionTask as a static member function
    static void detectionTask(void* parameter);

    // Helper methods for data handling
    byte calculateChecksum(byte *data, int length);
    void processBuffer(byte *buffer, unsigned long currentTime);
    void printRegularUpdate(byte *buffer, unsigned long currentTime);
    void handleDetectionStateChange(byte *buffer, bool newState, unsigned long currentTime);

    // New variable to track the status of the MMWave sensor
    bool _isStarted;

};

extern MMWave radar;
#endif // MMWAVE_H 