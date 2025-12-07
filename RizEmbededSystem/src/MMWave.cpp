#include "MMWave.h"
#include "soc/uart_reg.h"  // Add this for UART registers
#include "driver/uart.h"   // Add this for UART functions
#include "Log.h"           // Add this for logging system

extern MMWave radar;

MMWave::MMWave(uint8_t rx_pin, uint8_t tx_pin)
    : _rx_pin(rx_pin), _tx_pin(tx_pin), _serial(1),
      _targetDetected(false), _targetDistance(0), _objectInRange(false),
      _isRunning(false), _detectionTaskHandle(NULL),
      _isStarted(false) {
}

bool MMWave::checkConnections() {
    LOG_INFO(MODULE_MMWAVE, "Starting connection test");
    
    bool txConnected = false;
    bool rxConnected = false;
    bool communicationOK = false;

    // Test TX connection
    pinMode(_tx_pin, OUTPUT);
    pinMode(_rx_pin, INPUT_PULLUP);

    // TX Pin Test
    LOG_DEBUG(MODULE_MMWAVE, "Testing TX Pin %d", _tx_pin);
    digitalWrite(_tx_pin, HIGH);
    delay(10);
    int txHigh = digitalRead(_tx_pin);
    digitalWrite(_tx_pin, LOW);
    delay(10);
    int txLow = digitalRead(_tx_pin);

    if (txHigh == HIGH && txLow == LOW) {
        LOG_INFO(MODULE_MMWAVE, "TX Pin %d responding correctly", _tx_pin);
        txConnected = true;
    } else {
        LOG_WARN(MODULE_MMWAVE, "TX Pin not responding: HIGH=%d, LOW=%d", txHigh, txLow);
    }

    // RX Pin Test
    LOG_DEBUG(MODULE_MMWAVE, "Testing RX Pin %d", _rx_pin);
    int rxValue = digitalRead(_rx_pin);
    if (rxValue == HIGH) {
        LOG_INFO(MODULE_MMWAVE, "RX Pin %d detected (pulled up)", _rx_pin);
        rxConnected = true;
    } else {
        LOG_WARN(MODULE_MMWAVE, "RX Pin might be disconnected or shorted");
    }

    // UART Communication Test
    LOG_DEBUG(MODULE_MMWAVE, "Testing UART Communication");
    delay(100);
    
    // Flush any existing data
    while(_serial.available()) {
        _serial.read();
    }

    // Send test command
    _serial.write("test\r\n");
    
    // Wait for response
    unsigned long startTime = millis();
    while (millis() - startTime < 1000) {
        if (_serial.available()) {
            communicationOK = true;
            LOG_INFO(MODULE_MMWAVE, "Received response from sensor");
            
            String response = "Response: ";
            while (_serial.available()) {
                char c = _serial.read();
                char hexBuf[8];
                sprintf(hexBuf, "0x%02X ", c);
                response += hexBuf;
            }
            LOG_DEBUG(MODULE_MMWAVE, "%s", response.c_str());
            break;
        }
        delay(10);
    }

    if (!communicationOK) {
        LOG_WARN(MODULE_MMWAVE, "No response from sensor");
    }

    // Summary
    LOG_INFO(MODULE_MMWAVE, "Connection summary: TX=%s, RX=%s, UART=%s",
            txConnected ? "OK" : "FAIL",
            rxConnected ? "OK" : "FAIL",
            communicationOK ? "OK" : "FAIL");
    
    if (txConnected && rxConnected && communicationOK) {
        LOG_INFO(MODULE_MMWAVE, "All connections OK");
    } else {
        LOG_ERROR(MODULE_MMWAVE, "Connection issues detected");
        if (!txConnected) LOG_ERROR(MODULE_MMWAVE, "Check TX connection to pin %d", _tx_pin);
        if (!rxConnected) LOG_ERROR(MODULE_MMWAVE, "Check RX connection to pin %d", _rx_pin);
        if (!communicationOK) LOG_ERROR(MODULE_MMWAVE, "Verify sensor power and ground");
    }

    return (txConnected && rxConnected && communicationOK);
}

void MMWave::begin() {
    // First check connections
    if (!checkConnections()) {
        LOG_ERROR(MODULE_MMWAVE, "MMWave connection check failed!");
        return;
    }

    _serial.begin(115200, SERIAL_8N1, _rx_pin, _tx_pin);
    _serial.setRxBufferSize(1024);
    
    if (!_serial) {
        LOG_ERROR(MODULE_MMWAVE, "Failed to initialize radar serial!");
        return;
    }

    // Don't automatically start detection
    // startDetection() will be called when lights turn on
    _isRunning = false;
    _targetDetected = false;
    _objectInRange = false;
}

void MMWave::startDetection() {
    _isStarted = true;
    if (!_isRunning) {
        LOG_INFO(MODULE_MMWAVE, "Starting MMWave detection task");
        
        // MUCH more aggressive buffer clearing
        LOG_INFO(MODULE_MMWAVE, "Performing complete buffer reset before detection");
        
        // First, completely empty the buffer with multiple strategies
        int bytesCleared = 0;
        unsigned long startTime = millis();
        
        // 1. Hardware flush (clear UART FIFO directly)
        _serial.flush();
        
        // 2. Try to drain the buffer using read with timeout
        while (_serial.available() && (millis() - startTime < 150)) { // Longer timeout
            _serial.read();
            bytesCleared++;
            
            if (bytesCleared % 50 == 0) {
                LOG_DEBUG(MODULE_MMWAVE, "Cleared %d bytes so far...", bytesCleared);
            }
        }
        
        // 3. If buffer still has data, try more extreme measures
        if (_serial.available() > 0) {
            LOG_WARN(MODULE_MMWAVE, "Buffer still has %d bytes, performing hard reset", _serial.available());
            
            // End and restart serial connection
            _serial.end();
            delay(10);
            _serial.begin(115200, SERIAL_8N1, _rx_pin, _tx_pin);
            _serial.setRxBufferSize(1024);
            
            // Final verification
            bytesCleared += _serial.available();
            while (_serial.available()) {
                _serial.read();
            }
        }
        
        LOG_INFO(MODULE_MMWAVE, "Buffer reset complete: cleared %d bytes, %d remain", 
                bytesCleared, _serial.available());
        
        // Now set flags and create task
        _isRunning = true;
        _targetDetected = false;
        _objectInRange = false;
        _targetDistance = 0;
        _signalStrength = 0;
        
        // Create detection task
        BaseType_t result = xTaskCreatePinnedToCore(
            detectionTask,
            "DetectionTask",
            4096,
            this,
            configMAX_PRIORITIES - 1,
            &_detectionTaskHandle,
            0
        );
        
        if (result != pdPASS) {
            LOG_ERROR(MODULE_MMWAVE, "Failed to create detection task! Error code: %d", result);
            _isRunning = false;
        } else {
            LOG_DEBUG(MODULE_MMWAVE, "Detection task created successfully");
        }
    } else {
        LOG_WARN(MODULE_MMWAVE, "Detection already running!");
    }
}

void MMWave::stopDetection() {
    _isStarted = false;
    if (_isRunning) {
        LOG_INFO(MODULE_MMWAVE, "Stopping radar detection");
        
        // First signal the task to stop
        _isRunning = false;
        
        if (_detectionTaskHandle != NULL) {
            LOG_DEBUG(MODULE_MMWAVE, "Waiting for task completion");
            // Wait longer for task to complete current cycle
            vTaskDelay(pdMS_TO_TICKS(100));  // Increased delay
            
            LOG_DEBUG(MODULE_MMWAVE, "Storing task handle");
            // Store handle and verify it's valid
            TaskHandle_t tempHandle = _detectionTaskHandle;
            if (eTaskGetState(tempHandle) != eDeleted) {
                _detectionTaskHandle = NULL;
                
                LOG_DEBUG(MODULE_MMWAVE, "Resetting states");
                // Reset states
                _objectInRange = false;
                _targetDetected = false;
                _targetDistance = 0;
                
                LOG_DEBUG(MODULE_MMWAVE, "Suspending task");
                vTaskSuspend(tempHandle);  // Suspend before delete
                
                LOG_DEBUG(MODULE_MMWAVE, "Deleting task");
                // Delete task using local handle
                vTaskDelete(tempHandle);
                LOG_INFO(MODULE_MMWAVE, "Task deleted successfully");
            } else {
                LOG_WARN(MODULE_MMWAVE, "Task already deleted");
                _detectionTaskHandle = NULL;
            }
        } else {
            LOG_WARN(MODULE_MMWAVE, "No task handle found");
        }
    } else {
        LOG_DEBUG(MODULE_MMWAVE, "Detection already stopped");
    }
}

void MMWave::detectionTask(void* parameter) {
    LOG_DEBUG(MODULE_MMWAVE, "Detection task started");
    
    MMWave* radar = static_cast<MMWave*>(parameter);
    byte buffer[18];  // Local buffer for this task instance
    int bufferIndex = 0;
    bool lastInRange = false;
    
    // Add initialization verification
    LOG_DEBUG(MODULE_MMWAVE, "Task initialized with signal threshold: %d", 
              radar->_expectedSignalStrength);

    unsigned long lastHeartbeat = 0;
    unsigned long frameCount = 0;
    unsigned long emptyLoopCount = 0;
    unsigned long syncErrorCount = 0;
    int consecutiveMotionFrames = 0;
    
    // Syncing flags to get into proper state
    bool needSync = true;
    unsigned long lastSyncAttempt = 0;
    
    while (radar->_isRunning) {
        unsigned long now = millis();
        
        // Heartbeat logging
        if (now - lastHeartbeat > 1000) {
            LOG_DEBUG(MODULE_MMWAVE, "Task heartbeat: Frames:%lu Empty:%lu Sync:%lu Bytes:%d", 
                      frameCount, emptyLoopCount, syncErrorCount, radar->_serial.available());
            lastHeartbeat = now;
            frameCount = 0;
            emptyLoopCount = 0;
            syncErrorCount = 0;
            
            // Verify we're getting data
            if (radar->_serial.available() == 0) {
                LOG_WARN(MODULE_MMWAVE, "No data received for 1 second - sensor may be inactive");
            }
            
            // Emergency buffer reset if needed
            if (radar->_serial.available() > 200) {
                LOG_WARN(MODULE_MMWAVE, "Buffer overflow (%d bytes) - resetting sync", 
                         radar->_serial.available());
                bufferIndex = 0;
                needSync = true;
                
                // Emergency buffer drain (only read some bytes to avoid blocking)
                int drain = 0;
                while (radar->_serial.available() > 100 && drain < 100) {
                    radar->_serial.read();
                    drain++;
                }
                LOG_DEBUG(MODULE_MMWAVE, "Drained %d bytes, %d remain", 
                          drain, radar->_serial.available());
            }
        }
        
        // Re-sync if needed periodically
        if (needSync && (now - lastSyncAttempt > 500)) {
            LOG_DEBUG(MODULE_MMWAVE, "Attempting to sync with sensor data stream");
            lastSyncAttempt = now;
            
            // Drain buffer to find sync bytes
            int searchLimit = min(radar->_serial.available(), 100);
            for (int i = 0; i < searchLimit; i++) {
                byte b = radar->_serial.read();
                if (b == 0x55) {
                    // Potentially found start byte
                    if (radar->_serial.available() > 0) {
                        if (radar->_serial.peek() == 0xA5) {
                            // Found sync sequence
                            LOG_DEBUG(MODULE_MMWAVE, "Found sync bytes after %d bytes", i);
                            bufferIndex = 0;
                            buffer[bufferIndex++] = b;  // Store the 0x55
                            needSync = false;
                            break;
                        }
                    }
                }
            }
            
            if (needSync) {
                LOG_WARN(MODULE_MMWAVE, "Failed to find sync bytes in %d bytes", searchLimit);
                syncErrorCount++;
            }
        }
        
        // Process available data
        int bytesAvailable = radar->_serial.available();
        
        if (bytesAvailable == 0) {
            emptyLoopCount++;
            vTaskDelay(1);  // Short delay when no data
            continue;
        }
        
        // Process all available bytes with improved sync handling
        int maxBytesToProcess = 200;  // Increased from 100
        int bytesProcessed = 0;
        
        while (radar->_serial.available() > 0 && bytesProcessed < maxBytesToProcess) {
            bytesProcessed++;
            byte incomingByte = radar->_serial.read();
            
            // Sync handling
            if (bufferIndex == 0) {
                if (incomingByte == 0x55) {
                    buffer[bufferIndex++] = incomingByte;
                }
                continue;
            }
            
            if (bufferIndex == 1) {
                if (incomingByte == 0xA5) {
                    buffer[bufferIndex++] = incomingByte;
                } else {
                    bufferIndex = 0;  // Reset and look for 0x55 again
                    syncErrorCount++;
                }
                continue;
            }
            
            // Add byte to buffer
            buffer[bufferIndex++] = incomingByte;
            
            // Process complete frame
            if (bufferIndex >= 18) {
                frameCount++;
                
                // Verify checksum
                byte checksum = 0;
                for (int i = 0; i < 17; i++) {
                    checksum += buffer[i];
                }
                
                if (checksum != buffer[17]) {
                    LOG_WARN(MODULE_MMWAVE, "Checksum error, resetting sync");
                    bufferIndex = 0;
                    needSync = true;
                    continue;
                }
                
                // Parse data from valid frame
                uint16_t currentDistance = ((buffer[9] << 8) | buffer[10]);
                uint16_t currentSignal = ((buffer[15] << 8) | buffer[16]);
                byte presenceFlag = buffer[8];
                
                // Reset for next frame
                bufferIndex = 0;
                
                // Process presence state
                const char* presenceStatus;
                switch (presenceFlag) {
                    case 0: 
                        presenceStatus = "NO_TARGET"; 
                        consecutiveMotionFrames = 0;
                        break;
                    case 1: 
                        presenceStatus = "MOTION"; 
                        consecutiveMotionFrames++;
                        break;
                    case 2: 
                        presenceStatus = "PRESENCE"; 
                        consecutiveMotionFrames++;
                        break;
                    default: 
                        presenceStatus = "UNKNOWN"; 
                        break;
                }
                
                // Check if in range
                bool signalOK = (currentSignal >= radar->_expectedSignalStrength);
                bool distanceOK = (currentDistance <= 150 && currentDistance >= 10);
                
                // Determine detection state with improved logic
                bool newInRange = false;
                
                // Strong signal immediately triggers detection
                if (signalOK && distanceOK && presenceFlag != 0) {
                    newInRange = true;
                }
                // Multiple consecutive motion frames also trigger detection
                else if (consecutiveMotionFrames >= 3 && 
                         currentSignal >= (radar->_expectedSignalStrength * 0.7) && 
                         distanceOK && presenceFlag != 0) {
                    newInRange = true;
                }
                
                // Update class members
                radar->_signalStrength = currentSignal;
                radar->_targetDistance = currentDistance;
                
                // Always log sensor data
                LOG_INFO(MODULE_MMWAVE, "SENSOR: P:%s|SS:%d(%d)|TD:%d|MF:%d|InRange:%d->%d|Bytes:%d",
                          presenceStatus,
                          currentSignal, radar->_expectedSignalStrength,
                          currentDistance,
                          consecutiveMotionFrames,
                          lastInRange, newInRange,
                          radar->_serial.available());
                
                // Handle state change
                if (newInRange != lastInRange) {
                    lastInRange = newInRange;
                    radar->_objectInRange = newInRange;
                    
                    if (newInRange) {
                        LOG_INFO(MODULE_MMWAVE, "🔍 Object DETECTED - Trigger light off!");
                        
                        // Clean exit - clear remaining bytes
                        while (radar->_serial.available() > 0) {
                            radar->_serial.read();
                        }
                        break;
                    }
                }
            }
        }
    }
    
    // Final buffer cleanup before exit
    LOG_DEBUG(MODULE_MMWAVE, "Cleaning up %d remaining bytes", radar->_serial.available());
    while (radar->_serial.available() > 0) {
        radar->_serial.read();
    }
    
    LOG_INFO(MODULE_MMWAVE, "Detection task ending");
    vTaskDelete(NULL);
}

bool MMWave::objectDetected() {
    // Only check signal strength and distance
    return _isRunning && 
           (_signalStrength >= _expectedSignalStrength) &&
           (_targetDistance <= _expectedDistance);
}

void MMWave::processBuffer(byte *buffer, unsigned long currentTime) {
    // Motion status is in byte 8 (0-based index)
    byte motionStatus = buffer[8];
    
    // Distance is in bytes 9-10, in centimeters
    uint16_t rawDistance = ((buffer[9] << 8) | buffer[10]);
    _targetDistance = (rawDistance > 1000) ? 0 : rawDistance;
    
    // Signal strength is in bytes 15-16
    uint16_t rawSignal = ((buffer[15] << 8) | buffer[16]);
    _signalStrength = (rawSignal > 1000) ? 0 : rawSignal;
    
    _targetDetected = (motionStatus != 0);

    // Update object in range status
    _objectInRange = _targetDetected && 
                    (_signalStrength >= _expectedSignalStrength) &&
                    (_targetDistance <= _expectedDistance);  // Changed from >= to <=

    // Debug print with object in range status
    // Serial.printf("BTime:%lu|SS:%d(%d)|TD:%d(%d)|InRange:%d\n",
    //               currentTime,
    //               _signalStrength, _expectedSignalStrength,
    //               _targetDistance, _expectedDistance,
    //               _objectInRange);
}

void MMWave::printRegularUpdate(byte *buffer, unsigned long currentTime) {
    // Interpret presence flag with 3 states
    byte presenceFlag = buffer[8];
    const char* presenceStatus;
    switch (presenceFlag) {
        case 0: presenceStatus = "NO_TARGET"; break;
        case 1: presenceStatus = "MOTION"; break;
        case 2: presenceStatus = "PRESENCE"; break;
        default: presenceStatus = "UNKNOWN"; // Handle unexpected values
    }

    uint16_t distance = (buffer[9] << 8) | buffer[10];
    uint16_t signal = (buffer[15] << 8) | buffer[16];
    bool inRange = (presenceFlag != 0) && (signal >= _expectedSignalStrength);
    
    LOG_DEBUG(MODULE_MMWAVE, "P:%s|Dist:%d cm|Sig:%d|State:%s", 
              presenceStatus,
              distance,
              signal,
              inRange ? "IN_RANGE" : "OUT_RANGE");
}

void MMWave::handleDetectionStateChange(byte *buffer, bool newState, unsigned long currentTime) {
    if (newState) {
        LOG_INFO(MODULE_MMWAVE, "DETECTION EVENT at %lu ms", currentTime);
        LOG_DEBUG(MODULE_MMWAVE, "Raw Data: Status:0x%02X Dist:0x%02X%02X Strength:0x%02X%02X",
            buffer[8], buffer[9], buffer[10], buffer[15], buffer[16]);
        LOG_DEBUG(MODULE_MMWAVE, "Parsed: Distance:%.2fm Strength:%d",
            _targetDistance / 100.0, _signalStrength);
    } else {
        LOG_INFO(MODULE_MMWAVE, "Target lost at %lu ms", currentTime);
    }
}

void MMWave::update() {
    static byte buffer[18];
    static int bufferIndex = 0;

    while (_serial.available()) {
        byte incomingByte = _serial.read();

        if (bufferIndex == 0 && incomingByte != 0x55) {
            continue;
        }
        if (bufferIndex == 1 && incomingByte != 0xA5) {
            bufferIndex = 0;
            continue;
        }

        buffer[bufferIndex++] = incomingByte;

        if (bufferIndex >= 18) {
            bufferIndex = 0;
            unsigned long currentTime = millis();

            byte checksum = calculateChecksum(buffer, 17);
            if (checksum != buffer[17]) {
                continue;
            }

            processBuffer(buffer, currentTime);
            printRegularUpdate(buffer, currentTime);
        }
    }
}

byte MMWave::calculateChecksum(byte *data, int length) {
    byte sum = 0;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

void MMWave::setExpectedSignalStrength(int val){
    _expectedSignalStrength = val;
}

void MMWave::setExpectedDistance(int val){
    _expectedDistance = val;
}

void MMWave::setExpectedDelay(int val){
    _expectedDelay = val;
}

int MMWave::getExpectedSignalStrength() {
    return _expectedSignalStrength;
}

int MMWave::getExpectedDistance() {
    return _expectedDistance;
}

int MMWave::getExpectedDelay() {
    return _expectedDelay;
}

int MMWave::getSignalStrength() {
    return _signalStrength;
}

uint16_t MMWave::getExpectedSignalStrength() const {
    return _expectedSignalStrength;
}

bool MMWave::isStarted() const {
    return _isStarted;
}

bool MMWave::init() {
    LOG_INFO(MODULE_MMWAVE, "Initializing MMWave sensor");
    
    // Start with basic initialization
    _serial.begin(115200, SERIAL_8N1, _rx_pin, _tx_pin);
    begin();
    
    // Check if connections are good
    if (!checkConnections()) {
        LOG_ERROR(MODULE_MMWAVE, "Sensor connection check failed!");
        return false;
    }
    
    // Set default parameters - LOWERING THRESHOLD SIGNIFICANTLY for better sensitivity
    _expectedSignalStrength = 150;  // Reduced from 400 to 150 for much better sensitivity
    _expectedDistance = 200;        // Default distance threshold (cm)
    _expectedDelay = 100;           // Default delay (ms)
    
    LOG_DEBUG(MODULE_MMWAVE, "Clearing pending data");
    // Test sensor communication
    _serial.flush();
    while(_serial.available()) {
        _serial.read();  // Clear any pending data
    }
    
    // Try to get a valid reading
    unsigned long startTime = millis();
    bool validDataReceived = false;
    int bytesAvailable = 0;
    byte header1 = 0, header2 = 0;
    
    LOG_DEBUG(MODULE_MMWAVE, "Waiting for data frame");
    
    while (millis() - startTime < 2000) {  // 2 second timeout
        bytesAvailable = _serial.available();
        if (bytesAvailable > 0) {
            LOG_DEBUG(MODULE_MMWAVE, "Bytes available: %d", bytesAvailable);
            
            // Read and print all available bytes for debugging
            String byteStr = "Received bytes: ";
            for(int i = 0; i < min(bytesAvailable, 18); i++) {
                byte b = _serial.read();
                char hexBuf[8];
                sprintf(hexBuf, "0x%02X ", b);
                byteStr += hexBuf;
                
                // Check for header sequence
                if (i == 0) header1 = b;
                if (i == 1) header2 = b;
            }
            LOG_DEBUG(MODULE_MMWAVE, "%s", byteStr.c_str());
            
            if (header1 == 0x55 && header2 == 0xA5) {
                validDataReceived = true;
                LOG_INFO(MODULE_MMWAVE, "Valid header sequence found");
                break;
            } else {
                LOG_WARN(MODULE_MMWAVE, "Invalid headers: 0x%02X 0x%02X (expected: 0x55 0xA5)", 
                          header1, header2);
            }
        }
        delay(100);  // Increased delay to reduce serial spam
    }
    
    if (!validDataReceived) {
        LOG_ERROR(MODULE_MMWAVE, "Sensor initialization failed");
        LOG_DEBUG(MODULE_MMWAVE, "Last received headers: 0x%02X 0x%02X", header1, header2);
        LOG_ERROR(MODULE_MMWAVE, "Check power supply (5V) and TX/RX connections");
        return false;
    }
    
    LOG_INFO(MODULE_MMWAVE, "Sensor initialization successful");
    LOG_INFO(MODULE_MMWAVE, "MMWave configuration: SignalThreshold=%d, DistanceRange=10-%dcm", 
             _expectedSignalStrength, _expectedDistance);
    return true;
}

bool MMWave::isObjectInRange() {
    // Check for early termination to improve responsiveness
    if (!_isRunning) {
        return false;
    }
    
    return _objectInRange;
}