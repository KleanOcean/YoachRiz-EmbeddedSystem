#include "TF_Luna_UART.h"
#include "Log.h"

/**
 * Constructor: Initialize buffers and store pin configuration
 */
TF_Luna_UART::TF_Luna_UART(HardwareSerial* serial, int rx_pin, int tx_pin) 
    : _serial(serial), _rx_pin(rx_pin), _tx_pin(tx_pin) {
    xLidarMutex = xSemaphoreCreateMutex();
    memset(distanceBuffer, 0, sizeof(distanceBuffer));
    memset(amplitudeBuffer, 0, sizeof(amplitudeBuffer));
    memset(amplitudeHistory, 0, sizeof(amplitudeHistory));
    
    // Initialize cooldown start with current time
    cooldownStart = millis();
}

/**
 * Initialize sensor communication
 * Returns: true if successful, false otherwise
 */
bool TF_Luna_UART::begin(uint32_t baudRate) {
    _serial->setRxBufferSize(1024);
    _serial->begin(baudRate, SERIAL_8N1, _rx_pin, _tx_pin, true);
    delay(100);
    
    if (!tryConnect()) {
        return false;
    }

    for(int i=0; i<DYNAMIC_BASELINE_HISTORY_SIZE; i++) {
        amplitudeHistory[i] = baseline_amplitude;
    }
    historyFilled = false;
    historyIndex = 0;
    
    return true;
}

/**
 * Try different baud rates to establish communication
 * Returns: true if connection successful, false otherwise
 */
bool TF_Luna_UART::tryConnect() {
    const uint32_t baudRates[] = {921600};
    bool connected = false;
    
    for (uint32_t baud : baudRates) {
        LOG_DEBUG(MODULE_TOF, "Trying baud rate: %d", baud);
        
        _serial->end();
        delay(100);
        _serial->begin(baud, SERIAL_8N1, _rx_pin, _tx_pin);
        delay(100);
        
        // Test communication with firmware version request
        uint8_t test_cmd[] = {0x5A, 0x04, 0x01, 0x5F, 0x00};
        _serial->write(test_cmd, 5);
        
        // Wait for response with timeout
        unsigned long startTime = millis();
        while (millis() - startTime < 1000) {
            if (_serial->available()) {
                LOG_INFO(MODULE_TOF, "Got response at %d baud", baud);
                connected = true;
                break;
            }
            delay(10);
        }
        
        if (connected) break;
    }
    
    // Print troubleshooting info if connection fails
    if (!connected) {
        LOG_ERROR(MODULE_TOF, "Failed to get response at any baud rate");
        LOG_ERROR(MODULE_TOF, "Please check: Power supply (5V), TX/RX connections, Ground connection");
        return false;
    }
    
    return true;
}

/**
 * Configure sensor settings
 * - Enables continuous output
 * - Saves settings to flash
 */

bool TF_Luna_UART::configure() {
    // 100Hz command: 0x5A 0x06 0x03 0x60 0x00 0x00
    // 250Hz command: 0x5A 0x06 0x03 0xFA 0x00 0x00
    // 500Hz command: 0x5A 0x06 0x03 0xF4 0x01 0x00
    //Save command: 0x5A 0x04 0x11 0x00
    //here we set 250Hz
    uint8_t freq_cmd[] = {0x5A, 0x06, 0x03, 0xFA, 0x00, 0x00};
    _serial->write(freq_cmd, sizeof(freq_cmd));
    delay(50);
    
    // Save settings
    uint8_t save_cmd[] = {0x5A, 0x04, 0x11, 0x00};
    _serial->write(save_cmd, sizeof(save_cmd));
    delay(100);
    
    return true;
}


uint16_t TF_Luna_UART::updateLidarData() {
    static unsigned long lastReport = 0;
    static bool wasCooldownActive = true;

    if (!isRunning) {
        while(_serial->available()) _serial->read();
        return 0;
    }

    // NOTE: Do NOT reset cooldown here - it should only be reset when detection occurs
    // Removing the problematic resetCooldown() that was causing immediate cooldown activation
    
    // Store the current cooldown state - THIS MUST BE PRESERVED!
    bool currentCooldownActive = isCooldownActive();
    
    // IMPORTANT: We're only going to reset amplitude history and data processing
    // variables, NOT cooldown state
    
    // Only reset data processing variables, NOT cooldown!
    for(int i = 0; i < DYNAMIC_BASELINE_HISTORY_SIZE; i++) {
        amplitudeHistory[i] = 0;
    }
    historyFilled = false;
    historyIndex = 0;
    oldestIndex = 0;
    bufferIndex = 0;
    runningSum = 0;
    framesProcessed = 0;
    instantPercent = 0.0f;
    lastInstantPercent = 0;
    
    // Only reset max/min when cooldown finishes
    if (wasCooldownActive && !currentCooldownActive) {
        LOG_DEBUG(MODULE_TOF, "Cooldown finished - Resetting max/min percentages");
        maxPositivePercent = -INFINITY;  // Start at minimum possible value
        maxNegativePercent = INFINITY;   // Start at maximum possible value
    }
    wasCooldownActive = currentCooldownActive; // Update previous cooldown state

    // Log current cooldown state only when it's ACTIVE (reduced noise)
    if (currentCooldownActive) {
        LOG_DEBUG(MODULE_TOF, "Starting measurement with cooldown ACTIVE");
    }
    
    bool detectionTriggered = false;
    int extraFrameCounter = 0;
    // long startTime = micros();
    unsigned long iterationStart = micros();
    // unsigned long lastFrameCheck = micros();
    uint16_t currentAmp = 0;
    uint16_t maxAmp = 0;
    // takeBaseline();
    // takeBaseline(false);
    while (isRunning) {
        // Record start time of each iteration
        // unsigned long iterationStartTime = micros();
        
        // Cooldown state transition check - FIXED to only trigger once
        static bool lastCooldownState = currentCooldownActive;
        bool currentCD = isCooldownActive();

        // Check if cooldown just finished during measurement (only log once)
        if (lastCooldownState && !currentCD) {
            LOG_DEBUG(MODULE_TOF, "Cooldown finished - Resetting max/min percentages");
            maxPositivePercent = -INFINITY;
            maxNegativePercent = INFINITY;
            lastCooldownState = currentCD;  // Update state immediately to prevent repeat
        } else if (lastCooldownState != currentCD) {
            lastCooldownState = currentCD;  // Update state on any transition
        }

        // Check available bytes
        if (_serial->available() < 9) {
            // unsigned long now = micros();
            // If less than 5ms since last check, wait a bit
            // if (now - lastFrameCheck < 5000) {
            //     delayMicroseconds(500); // Small delay to prevent busy waiting
            //     continue;   }
            // // If no data for too long, maybe exit
            // if (now - lastFrameCheck > 50000) { // 50ms timeout
            //   break;  }
            continue;
        }
        // lastFrameCheck = micros();
        // iterationStart = micros();
        
        uint8_t header1 = _serial->read();
        if (header1 != 0x59) {    continue;  }
        uint8_t header2 = _serial->peek();
        if (header2 != 0x59) {   continue; }

        uint8_t frame[9];
        frame[0] = header1;
        _serial->readBytes(&frame[1], 8);


        if (parseFrame(frame)) {
            currentAmp = Lidar.u16Amp;
            // timestamp
            uint16_t timestamp = millis();

            // Track if we just exited cooldown to prevent using stale baseline data
            static bool wasCooldownActive = false;
            static int framesAfterCooldown = 0;
            bool currentCooldown = isCooldownActive();

            // Detect cooldown state change
            if (wasCooldownActive && !currentCooldown) {
                framesAfterCooldown = 0; // Reset counter when cooldown ends
                LOG_DEBUG(MODULE_TOF, "Cooldown just ended - will wait for %d fresh frames before updating baseline",
                          DYNAMIC_BASELINE_HISTORY_SIZE);
            }
            wasCooldownActive = currentCooldown;

            // Count frames after cooldown ends
            if (!currentCooldown && framesAfterCooldown < DYNAMIC_BASELINE_HISTORY_SIZE) {
                framesAfterCooldown++;
            }

            // if framesProcessed > 100, compute new baseline dynamically
            // BUT ONLY when:
            // 1. Not in cooldown
            // 2. Have accumulated enough fresh frames after cooldown ends (to avoid stale data)
            if (framesProcessed > DYNAMIC_BASELINE_HISTORY_SIZE &&
                !currentCooldown &&
                framesAfterCooldown >= DYNAMIC_BASELINE_HISTORY_SIZE)
            {
                baseline_amplitude = computeDynamicBaseline();
                if (baseline_amplitude == 0) { baseline_amplitude = 1;} // dun want 0
                amplitude_threshold = (uint16_t)(baseline_amplitude * amplitude_threshold_factor);
             }

            // Ensure baseline is never zero to prevent division by zero
            if (baseline_amplitude == 0) {
                baseline_amplitude = 1;
            }

            // Calculate percentage difference from baseline (now safe from division by zero)
            float percentageDiff = ((float)currentAmp - baseline_amplitude) / (float)baseline_amplitude * 100.0f;

            // Track max/min percentage changes
            instantPercent = percentageDiff;
            if (percentageDiff > maxPositivePercent) {
                maxPositivePercent = percentageDiff;
            }
            if (percentageDiff < maxNegativePercent) {
                maxNegativePercent = percentageDiff;
            }

            // Compare percentage difference against threshold percentage
            if (abs(percentageDiff) > (amplitude_threshold_factor - 1) * 100.0f) {
                // First check and log cooldown status to debug the issue
                LOG_DEBUG(MODULE_TOF, "Amplitude threshold exceeded: %d vs %d (%.2f%%), Cooldown: %s",
                          currentAmp, baseline_amplitude, percentageDiff, 
                          isCooldownActive() ? "ACTIVE" : "INACTIVE");
                
                if (!detectionTriggered && !isCooldownActive()) {
                    LOG_INFO(MODULE_TOF, "Detection triggered at frame %d, amplitude: %d, baseline: %d, diff: %.2f%%", 
                            framesProcessed, currentAmp, baseline_amplitude, percentageDiff);
                    
                    detectionTriggered = true;
                    extraFrameCounter = 0;
                    resetCooldown(); // Reset cooldown timer
                    continue;
                } else if (isCooldownActive()) {
                    LOG_DEBUG(MODULE_TOF, "Detection suppressed by cooldown (%lu ms remaining)",
                              cooldownDuration - (millis() - cooldownStart));
                }
            }
            // else {
            //     Serial.printf("Detection not triggered at framesProcessed: %d\n", framesProcessed);
            //     Serial.printf("amplitude threshold: %d\n", amplitude_threshold);
            //     Serial.printf("Curr Amp: %d\n", currentAmp);
            //     Serial.printf("baseline_amplitude: %d\n", baseline_amplitude);
            //     Serial.printf("isCooldownActive: %d\n", isCooldownActive());
            //     Serial.printf("abs(Lidar.u16Amp - baseline_amplitude): %d\n", abs(currentAmp - baseline_amplitude));
            //     Serial.printf("timestamp: %d\n", timestamp);
            // }

            // Serial.printf("[%d ms] s1.5: %.2f ms\n", millis(),  (micros() - iterationStart) / 1000.0f);

            if (detectionTriggered) {
                // Serial.printf("[%d ms] s1.6: %.2f ms\n", millis(),  (micros() - iterationStart) / 1000.0f);
                // Serial.printf("Extra Frame %d: Cur:%d | Bas:%d | timestamp: %d\n",
                //               extraFrameCounter+1, Lidar.u16Amp, baseline_amplitude, timestamp);
                extraFrameCounter++;
                if (extraFrameCounter >= 1) {
                    // Serial.printf("[%d ms] s1.7: %.2f ms\n", millis(),  (micros() - iterationStart) / 1000.0f);
                    detectedAmplitude = currentAmp;
                    detectionTimestamp = timestamp;
                    _objectDetected = true;

                    break;
                }
                // Serial.printf("[%d ms] s1.8: %.2f ms\n", millis(),  (micros() - iterationStart) / 1000.0f);
                continue;
            }
            // Serial.printf("[%d ms] s1.9: %.2f ms\n", millis(),  (micros() - iterationStart) / 1000.0f);
            Lidar.frame_complete = true;
            framesProcessed++;

            // Update buffers only for normal frames (not during detection trigger or cooldown)
            // This prevents abnormal amplitude values from polluting the baseline calculation
            if (!isCooldownActive()) {
                updateBuffers(Lidar.u16Distance, Lidar.u16Amp);
            }

            // Compute acceleration based on amplitude change per frame
            static unsigned long lastFrameTime = 0;
            static uint16_t lastAmp = 0;
            float amplitudeAcceleration = 0.0f;
            if (lastFrameTime != 0) {
                unsigned long dt = timestamp - lastFrameTime;
                if (dt > 0) {
                    amplitudeAcceleration = ((float)currentAmp - baseline_amplitude) * 1.0f / dt; // amplitude units per milli second
                }
            }
            lastAmp = currentAmp;
            lastFrameTime = timestamp;

        // also print the max amplitude, min amplitude, and instant percentage
        // if (currentAmp > maxAmp) { maxAmp = currentAmp; }
        // Serial.printf("maxAmp: %d\n", maxAmp);

        // Log every frame at 250Hz as requested
        LOG_DEBUG(MODULE_TOF, "db:%d,Cur:%d|Bas:%d|Thr:%.1f%%|+:%.2f%%|-:%.2f%%|I:%.2f%%|CD:%d",
                        _serial->available(),
                        currentAmp,
                        baseline_amplitude,
                        (amplitude_threshold_factor-1)*100.0f,
                        maxPositivePercent,
                        maxNegativePercent,
                        instantPercent,
                        isCooldownActive());

        // In updateLidarData, add frame data logging
        // LOG_DEBUG(MODULE_TOF, "Frame data: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        //          frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7], frame[8]);

        }
        // const unsigned long processingTime = micros() - iterationStart;
        // if(processingTime < 800) {
        //     delayMicroseconds(800 - processingTime);
        // }
        
        // Calculate how long this iteration took (in microseconds)
        // unsigned long iterationTime = micros() - iterationStartTime;
        
        // If iteration took less than 3ms (3000 microseconds), delay the difference
        // if (iterationTime < 2000) {
        //     delayMicroseconds(2000 - iterationTime);
        // }
    }

    
    return currentAmp;
}

void TF_Luna_UART::resetUpdateLidarData() {
    for(int i = 0; i < DYNAMIC_BASELINE_HISTORY_SIZE; i++) {
        amplitudeHistory[i] = 0;
    }
    historyFilled = false;
    historyIndex = 0;
    oldestIndex = 0;
    bufferIndex = 0;
    runningSum = 0;
    framesProcessed = 0;
    LOG_DEBUG(MODULE_TOF, "Resetting max/min percentages at %lu ms", millis());
    maxPositivePercent = -INFINITY;  // Reset to minimum
    maxNegativePercent = INFINITY;   // Reset to maximum
    instantPercent = 0.0f;
    cooldownStart = millis();
    lastInstantPercent = 0;
}

bool TF_Luna_UART::parseFrame(uint8_t* frame) {
    // Calculate checksum
    uint8_t checksum = 0;
    for (int i = 0; i < 8; i++) {
        checksum += frame[i];
    }
    if (checksum != frame[8]) return false;
    
    // Parse data
    Lidar.u16Distance = frame[2] | (frame[3] << 8);
    Lidar.u16Amp = frame[4] | (frame[5] << 8);
    Lidar.temperature = (frame[6] | (frame[7] << 8)) / 8 - 256;
    
    return true;
}

void TF_Luna_UART::updateBuffers(uint16_t distance, uint16_t amplitude) {
    // Validate amplitude range
    if(amplitude < 100 || amplitude > 6000) return;

    // Update running sum
    if(historyFilled) {
        runningSum -= amplitudeHistory[oldestIndex];
        oldestIndex = (oldestIndex + 1) % DYNAMIC_BASELINE_HISTORY_SIZE;
    }
    runningSum += amplitude;

    // Update main buffers
    distanceBuffer[bufferIndex] = distance;
    amplitudeBuffer[bufferIndex] = amplitude;
    bufferIndex = (bufferIndex + 1) % MOVING_AVG_SIZE;

    // Update history buffer
    amplitudeHistory[historyIndex] = amplitude;
    historyIndex = (historyIndex + 1) % DYNAMIC_BASELINE_HISTORY_SIZE;
    
    if(!historyFilled && historyIndex == 0) {
        historyFilled = true;
        oldestIndex = 0; // Reset oldest index when buffer first fills
    }
}

uint16_t TF_Luna_UART::computeDynamicBaseline() {
    uint16_t result;
    if(historyFilled) {
        result = runningSum / DYNAMIC_BASELINE_HISTORY_SIZE;
        LOG_DEBUG(MODULE_TOF, "computeDynamicBaseline: historyFilled=true, runningSum=%lu, size=%d, result=%d",
                  runningSum, DYNAMIC_BASELINE_HISTORY_SIZE, result);
    } else {
        result = historyIndex > 0 ? runningSum / historyIndex : 0;
        LOG_DEBUG(MODULE_TOF, "computeDynamicBaseline: historyFilled=false, runningSum=%lu, historyIndex=%d, result=%d",
                  runningSum, historyIndex, result);
    }
    return result;
}

u16 TF_Luna_UART::getMovingAverage(u16* buffer, uint8_t size) {
    u32 sum = 0;
    for (uint8_t i = 0; i < size; i++) {
        sum += buffer[i];
    }
    return sum / size;
}

bool TF_Luna_UART::isAmplitudeSpike(u16 currentAmp) {
    u16 avgAmp = 0;
    for (int i = 0; i < AMPLITUDE_SPIKE_HISTORY_SIZE; i++) {
        avgAmp += amplitudeHistory[i];
    }
    avgAmp /= AMPLITUDE_SPIKE_HISTORY_SIZE;

    // Add debug logging here
    LOG_DEBUG(MODULE_TOF, "Spike Check: CurAmp=%d, AvgAmp=%d, Diff=%d, Threshold=%d",
              currentAmp, avgAmp, abs(currentAmp - avgAmp), (int)(avgAmp * 0.5));

    return abs(currentAmp - avgAmp) > (avgAmp * 0.5);
}

void TF_Luna_UART::printLidarData() {
    LOG_DEBUG(MODULE_TOF, "D: %dcm, A: %d", Lidar.u16Distance, Lidar.u16Amp);
}

void TF_Luna_UART::takeBaseline(bool stop_reading) {
    LOG_INFO(MODULE_TOF, "========== TOF Calibration Start ==========");
    unsigned long calibrationStartTime = millis();

    baseline_amplitude = 0;
    int validSamples = 0;
    int totalFrames = 0;
    const int TOTAL_FRAMES_NEEDED = 50;
    const int START_FRAME = 40;

    // CRITICAL: Stop reading first to prevent TOF task from consuming data
    // The TOF task's updateLidarData() will consume UART data if isRunning=true
    stopReading();
    LOG_DEBUG(MODULE_TOF, "[Stage 0/5] Stopped background reading, isRunning=%d", isRunning);

    // Stage 1: Clear UART buffer with timeout protection
    LOG_INFO(MODULE_TOF, "[Stage 1/5] Clearing UART buffer...");
    unsigned long bufferClearStart = millis();
    int bytesCleared = 0;
    const unsigned long BUFFER_CLEAR_TIMEOUT = 100; // 100ms max

    // Clear buffer with timeout to prevent infinite loop
    // (Sensor continuously sends data at 250Hz)
    while(_serial->available() && (millis() - bufferClearStart < BUFFER_CLEAR_TIMEOUT)) {
        _serial->read();
        bytesCleared++;
    }

    unsigned long bufferClearTime = millis() - bufferClearStart;
    LOG_INFO(MODULE_TOF, "[Stage 1/5] Buffer cleared: %d bytes in %lu ms", bytesCleared, bufferClearTime);

    if (millis() - bufferClearStart >= BUFFER_CLEAR_TIMEOUT) {
        LOG_WARN(MODULE_TOF, "[Stage 1/5] ⚠️ Buffer clear timeout (sensor continuously sending)");
    }

    // Stage 2: Start reading from sensor (exclusively for calibration)
    LOG_INFO(MODULE_TOF, "[Stage 2/5] Starting sensor reading...");
    startReading();
    LOG_INFO(MODULE_TOF, "[Stage 2/5] Sensor reading started, isRunning=%d", isRunning);

    // Give sensor a moment to stabilize after clearing buffer
    delay(10);

    unsigned long startTime = millis();
    unsigned long timeout = 500; // 5 second timeout
    LOG_INFO(MODULE_TOF, "[Stage 3/5] Collecting %d frames (using frames %d-%d for baseline)...",
             TOTAL_FRAMES_NEEDED, START_FRAME + 1, TOTAL_FRAMES_NEEDED);
    LOG_INFO(MODULE_TOF, "[Stage 3/5] Timeout set to %lu ms", timeout);

    // Stage 3: Collect frames
    int invalidHeaders = 0;
    int parseFailures = 0;
    unsigned long lastProgressLog = startTime;

    while (totalFrames < TOTAL_FRAMES_NEEDED && (millis() - startTime < timeout)) {
        if (isRunning && _serial->available() >= 9) {
            uint8_t header1 = _serial->read();
            if (header1 != 0x59) {
                invalidHeaders++;
                continue;
            }

            uint8_t header2 = _serial->peek();
            if (header2 != 0x59) {
                invalidHeaders++;
                continue;
            }

            // We have a valid header, read the full frame
            uint8_t frame[9];
            frame[0] = header1;
            _serial->readBytes(&frame[1], 8);

            if (parseFrame(frame)) {
                totalFrames++;

                // Log progress every 10 frames or when collecting baseline samples
                if (totalFrames % 10 == 0 || totalFrames == START_FRAME + 1) {
                    unsigned long elapsed = millis() - startTime;
                    LOG_INFO(MODULE_TOF, "[Stage 3/5] Progress: %d/%d frames collected, elapsed: %lu ms (avg: %.1f ms/frame)",
                             totalFrames, TOTAL_FRAMES_NEEDED, elapsed, (float)elapsed / totalFrames);
                }

                // Only use frames after START_FRAME for baseline calculation
                if (totalFrames > START_FRAME) {
                    validSamples++;
                    baseline_amplitude += Lidar.u16Amp;

                    // Log first and last baseline sample
                    if (validSamples == 1 || validSamples == (TOTAL_FRAMES_NEEDED - START_FRAME)) {
                        LOG_DEBUG(MODULE_TOF, "[Stage 3/5] Baseline sample #%d: amplitude=%d, distance=%d",
                                 validSamples, Lidar.u16Amp, Lidar.u16Distance);
                    }
                }
            } else {
                parseFailures++;
            }
        }
        // No delay needed - let loop run at natural pace
        // Sensor outputs at 250Hz (4ms/frame), loop will sync naturally
    }

    unsigned long collectionTime = millis() - startTime;
    LOG_INFO(MODULE_TOF, "[Stage 3/5] Frame collection finished: %d frames in %lu ms", totalFrames, collectionTime);
    LOG_INFO(MODULE_TOF, "[Stage 3/5] Statistics: invalidHeaders=%d, parseFailures=%d", invalidHeaders, parseFailures);

    // Stage 4: Calculate baseline
    LOG_INFO(MODULE_TOF, "[Stage 4/5] Calculating baseline from samples...");

    // Handle timeout case
    if (totalFrames < TOTAL_FRAMES_NEEDED) {
        LOG_WARN(MODULE_TOF, "[Stage 4/5] ⚠️  Collection timeout! Only %d/%d frames collected", totalFrames, TOTAL_FRAMES_NEEDED);
        // Still calculate baseline with whatever frames we got, or use default
        if (validSamples > 0) {
            baseline_amplitude /= validSamples;
            LOG_WARN(MODULE_TOF, "[Stage 4/5] Using partial baseline from %d samples", validSamples);
        } else {
            baseline_amplitude = 100; // Default safe value
            LOG_ERROR(MODULE_TOF, "[Stage 4/5] No valid samples! Using default baseline=%d", baseline_amplitude);
        }
    } else {
        // Normal case: calculate average
        if (validSamples > 0) {
            baseline_amplitude /= validSamples;
            LOG_INFO(MODULE_TOF, "[Stage 4/5] ✓ Baseline calculated from %d samples: raw_sum=%d, average=%d",
                     validSamples, baseline_amplitude * validSamples, baseline_amplitude);
        } else {
            baseline_amplitude = 100;
            LOG_ERROR(MODULE_TOF, "[Stage 4/5] No valid samples despite collecting frames! Using default=%d", baseline_amplitude);
        }
    }

    if (totalFrames == 0) {
        LOG_ERROR(MODULE_TOF, "[Stage 4/5] ❌ CRITICAL: No frames received from sensor - check connections");
        // Try resetting the sensor or connection
        LOG_INFO(MODULE_TOF, "[Stage 4/5] Attempting sensor reset...");
        _serial->end();
        delay(100);
        _serial->begin(TOF_BAUD_RATE, SERIAL_8N1, _rx_pin, _tx_pin);
        LOG_INFO(MODULE_TOF, "[Stage 4/5] Sensor reset completed");
    }

    // Stage 5: Finalize
    LOG_INFO(MODULE_TOF, "[Stage 5/5] Finalizing calibration...");

    if (stop_reading) {
        stopReading();
        LOG_INFO(MODULE_TOF, "[Stage 5/5] Sensor reading stopped");
    } else {
        LOG_INFO(MODULE_TOF, "[Stage 5/5] Sensor reading continues");
    }

    // Calculate amplitude threshold based on baseline
    amplitude_threshold = (uint16_t)(baseline_amplitude * amplitude_threshold_factor);

    unsigned long totalCalibrationTime = millis() - calibrationStartTime;
    LOG_INFO(MODULE_TOF, "[Stage 5/5] ✓ Threshold calculated: baseline=%d, threshold=%d, factor=%.2f",
             baseline_amplitude, amplitude_threshold, amplitude_threshold_factor);
    LOG_INFO(MODULE_TOF, "========== TOF Calibration Complete ==========");
    LOG_INFO(MODULE_TOF, "Summary: %d samples from %d frames in %lu ms (%.1f ms/frame)",
             validSamples, totalFrames, totalCalibrationTime,
             totalFrames > 0 ? (float)totalCalibrationTime / totalFrames : 0.0f);
}

bool TF_Luna_UART::checkLidarDetection() {
    // Don't check during cooldown period
    if (isCooldownActive()) {
        return false;
    }

    unsigned long currentTime = millis();
    bool currentState = false;
    
    // Only check if cooldown has expired
    if (!isAmplitudeSpike(Lidar.u16Amp) && Lidar.u16Amp > amplitude_threshold) {
        currentState = true;
        resetCooldown();  // Reset cooldown when detection occurs
    }
    
    if (currentState != lastDetectionState) {
        if (currentTime - lastDetectionTime >= DEBOUNCE_TIME) {
            lastDetectionState = currentState;
            lastDetectionTime = currentTime;
        }
    }
    return lastDetectionState;
}

void TF_Luna_UART::syncFrames() {
    while(_serial->available() > 0) {
        if(_serial->read() == 0x59 && _serial->peek() == 0x59) break;
    }
}

bool TF_Luna_UART::init() {
    LOG_INFO(MODULE_TOF, "Initializing TF-Luna UART LiDAR");
    
    // Initialize cooldown start with current time
    cooldownStart = millis();
    
    // Initialize with default baud rate
    if (!begin(TOF_BAUD_RATE)) {
        LOG_ERROR(MODULE_TOF, "Failed to initialize TF-Luna sensor");
        LOG_ERROR(MODULE_TOF, "Check connections and restart");
        return false;
    }
    takeBaseline();
    LOG_INFO(MODULE_TOF, "TF-Luna initialization successful");
    return true;
}
