#include <Arduino.h>
#include "BluetoothControl.h"
#include "LightControl.h"
#include "DataControl.h"
#include "Pangodream_18650_CL.h"
#include "TF_Luna_UART.h"
// #include "MMWave.h"  // Disabled for now
#include <cmath> // Include cmath for logarithmic functions
#include "Global_VAR.h"
#include "esp_pm.h"
#include "Log.h"

#include "YMOTOR.h"
YMOTOR motors;

// ===== Version Control =====
#define FIRMWARE_VERSION "0.0.2"
#define FIRMWARE_VERSION_STRING "Yoach1 v0.0.2"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__

// Forward declarations for mutex helper functions
bool takeMutexWithLogging(SemaphoreHandle_t mutex, uint32_t timeout, const char* module, const char* mutexName);
void giveMutexWithLogging(SemaphoreHandle_t mutex, const char* module, const char* mutexName);

// Helper function to get mode name for logging
static const char* getModeName(int mode) {
    switch(mode) {
        case MANUAL_MODE: return "MANUAL";
        case RANDOM_MODE: return "RANDOM";
        case TIMED_MODE: return "TIMED";
        case DOUBLE_MODE: return "DOUBLE";
        case RHYTHM_MODE: return "RHYTHM";
        case MOVEMENT_MODE: return "MOVEMENT";
        case OPENING_MODE: return "OPENING";
        case CLOSING_MODE: return "CLOSING";
        case TERMINATE_MODE: return "TERMINATE";
        case RESTTIMESUP_MODE: return "RESTTIMESUP";
        case PROCESSED_MODE: return "PROCESSED";
        case CONFIG_MODE: return "CONFIG";
        default: return "UNKNOWN";
    }
}

/**
 * ===== Concurrency Control System =====
 * 
 * This system uses three primary mutexes to protect shared resources:
 * 
 * 1. xSensorMutex: Protects access to TOF sensor data and task flags
 *    - Used for: sensorData, hasTOFDetectionTask
 * 
 * 2. xMMWaveMutex: Protects radar control and detection operations
 *    - Used for: hasMMWaveDetectionTask, radar control operations
 * 
 * 3. xObjectDetectedMutex: Protects detection state flags
 *    - Used for: objectDetectedFlag
 * 
 * Helper functions for standardized mutex operations:
 * - takeMutexWithLogging(): Takes a mutex with timeout and logs the result
 * - giveMutexWithLogging(): Gives a mutex and logs the operation
 */

// ===== Constants =====
#define PROCESSING_HISTORY_SIZE 50

int colourCherryRed2[] =     {  121,6,4 };

// ===== Global Objects =====
BluetoothControl BLE;
LightControl LIGHT;
DataControl DATA;
Pangodream_18650_CL BL(35);
// MMWave radar(RADAR_RX_PIN, RADAR_TX_PIN);  // Disabled for now
HardwareSerial TOF_SERIAL(2);  // Use UART2
TF_Luna_UART TOF_SENSOR(&TOF_SERIAL, TOF_RX_PIN, TOF_TX_PIN);

// ===== Mutexes =====
SemaphoreHandle_t xSensorMutex;       // Protects sensor access
// SemaphoreHandle_t xMMWaveMutex;       // Protects MMWave radar control - Disabled for now
SemaphoreHandle_t xObjectDetectedMutex; // Protects detection flags

// ===== Sensor Data Structure =====
struct SensorData {
    uint16_t distance;
    uint16_t amplitude;
    bool detected;
    unsigned long timestamp;
    unsigned long detectionTimestamp;
};

// ===== Shared Variables =====
SensorData sensorData = {0, 0, false, 0, 0};    // Protected by xSensorMutex
bool hasTOFDetectionTask = false;               // Protected by xSensorMutex
// bool hasMMWaveDetectionTask = false;            // Protected by xMMWaveMutex - Disabled for now
bool objectDetectedFlag = false;                // Protected by xObjectDetectedMutex
static uint16_t amplitudeThreshold;             // Read-only after initialization

// ===== Timing and Performance Tracking =====
static unsigned long lastCheckTime = 0;
static unsigned long processingTimes[PROCESSING_HISTORY_SIZE] = {0};
static uint8_t processIndex = 0;
static uint16_t iterationCount = 0;

// ===== Button State Tracking =====
static int lastButtonState = HIGH;
static unsigned long lastDebounceTime = 0;
static const unsigned long debounceDelay = 20; // Reduced to 20ms
static unsigned long lastButtonLogTime = 0;
static const unsigned long buttonLogInterval = 1000; // 1 second

// ===== Forward Declarations =====
void handleGameMode(int gameMode);
void handleTimedMode();

/**
 * Handles the timed mode operation
 * Checks if we're within the timeout period and sends appropriate notifications
 */
void handleTimedMode() {
    static unsigned long lastTriggerTime = 0;
    unsigned long currentTime = millis();
    unsigned long timeout = DATA.getTimedBreak();

    if (currentTime - lastTriggerTime < timeout) {
        BLE.sendMsgAndNotify("timed");
        LOG_INFO(MODULE_DATA, "Timed mode active (Remaining: %lu ms)", timeout - (currentTime - lastTriggerTime));
    } else {
        BLE.sendMsgAndNotify("Timed Mode Overtimed");
        LOG_WARN(MODULE_DATA, "Timed mode overtimed at %lu ms", currentTime);
    }
    
    lastTriggerTime = currentTime;
}

/**
 * Handles different game modes and sends appropriate notifications
 */
void handleGameMode(int gameMode) {
    switch (gameMode) {
        case MANUAL_MODE:
            BLE.sendMsgAndNotify("manual");
            break;
        case RANDOM_MODE:
            BLE.sendMsgAndNotify("random");
            break;
        case RHYTHM_MODE:
            BLE.sendMsgAndNotify("rhythm");
            break;
        case DOUBLE_MODE:
            BLE.sendMsgAndNotify("double" + String(DATA.getDoubleModeIndex()));
            break;
        case TIMED_MODE:
            handleTimedMode();
            break;
        case CONFIG_MODE:
            LOG_INFO(MODULE_MAIN, "Configuring light with blink count: %d", DATA.getConfigBlinkCount());
            LIGHT.configNumberWipe(DATA.getConfigBlinkCount());
            BLE.sendMsgAndNotify("config:" + String(DATA.getConfigBlinkCount()));
            DATA.setGameMode(PROCESSED_MODE); // Auto-transition to PROCESSED_MODE after configuration
            break;
    }
}

/**
 * TOF Sensor Task - Core 1
 * Handles time-of-flight sensor readings in a non-blocking way
 */
void TOFSensorTask(void *parameter) {
    LOG_INFO(MODULE_TOF, "TOF Sensor task started");
    
    for (;;) {
        bool shouldRunTask = false;
        
        // Safely check task status using proper mutex
        if (takeMutexWithLogging(xSensorMutex, 10, MODULE_TOF, "Sensor")) {
            shouldRunTask = hasTOFDetectionTask;
            giveMutexWithLogging(xSensorMutex, MODULE_TOF, "Sensor");
            
            if (shouldRunTask) {
                LOG_DEBUG(MODULE_TOF, "Starting TOF measurement cycle");

                TOF_SENSOR.startReading();
                TOF_SENSOR.updateLidarData();
                
                // Reset task flag when done
                if (takeMutexWithLogging(xSensorMutex, 10, MODULE_TOF, "Sensor")) {
                    hasTOFDetectionTask = false;
                    giveMutexWithLogging(xSensorMutex, MODULE_TOF, "Sensor");
                    LOG_DEBUG(MODULE_TOF, "TOF measurement cycle completed");
                }
            }
        }
        
        // Sleep shorter when active, longer when idle
        vTaskDelay(shouldRunTask ? 1 : 10);
    }
}

/**
 * MMWave Sensor Task - Core 1
 * Handles radar detection in a non-blocking way
 * DISABLED FOR NOW
 */
/*
void MMWaveSensorTask(void *parameter) {
    LOG_INFO(MODULE_MMWAVE, "MMWave radar task started");
    
    // Track whether detection is running
    bool isDetectionActive = false;
    
    for (;;) {
        // Check for TERMINATE_MODE - highest priority check
        if (DATA.getGameMode() == TERMINATE_MODE && isDetectionActive) {
            LOG_INFO(MODULE_MMWAVE, "TERMINATE detected - stopping radar");
            radar.stopDetection();
            LIGHT.turnLightOff();
            isDetectionActive = false;
            vTaskDelay(10);
            continue;  // Skip to next iteration
        }
        
        // Safely check current state
        bool isTaskRequested = false;
        if (takeMutexWithLogging(xMMWaveMutex, 10, MODULE_MMWAVE, "MMWave")) {
            isTaskRequested = hasMMWaveDetectionTask;
            giveMutexWithLogging(xMMWaveMutex, MODULE_MMWAVE, "MMWave");
        }
        
        // Handle starting new detection
        if (isTaskRequested && !isDetectionActive) {
            LOG_INFO(MODULE_MMWAVE, "Starting radar detection");
            radar.startDetection();
            isDetectionActive = true;
            
            // Reset task request flag
            if (takeMutexWithLogging(xMMWaveMutex, 10, MODULE_MMWAVE, "MMWave")) {
                hasMMWaveDetectionTask = false;  // We've started the task
                giveMutexWithLogging(xMMWaveMutex, MODULE_MMWAVE, "MMWave");
                LOG_DEBUG(MODULE_MMWAVE, "MMWave detection started, task flag reset");
            }
        }
        
        // Process detection if active
        if (isDetectionActive) {
            // Check for object detection
            if (radar.isObjectInRange()) {
                LOG_INFO(MODULE_MMWAVE, "Object detected in range, stopping light and radar");
                LIGHT.turnLightOff();
                radar.stopDetection();
                isDetectionActive = false;
                
                // Check if we're in Rhythm Mode
                if (DATA.prevGameMode == RHYTHM_MODE) {
                    LOG_INFO(MODULE_MMWAVE, "Rhythm Mode detection by MMWave");
                    handleGameMode(RHYTHM_MODE); // This will send "rhythm" notification
                } else {
                    LOG_INFO(MODULE_MMWAVE, "Timed Mode triggered by detection");
                    handleGameMode(TIMED_MODE);
                }
            }
            
            // Check for termination while detection is active
            if (DATA.getGameMode() == TERMINATE_MODE) {
                LOG_INFO(MODULE_MMWAVE, "TERMINATE during detection - stopping radar");
                radar.stopDetection();
                isDetectionActive = false;
            }
            
            vTaskDelay(1);
        } else {
            vTaskDelay(10);
        }
    }
}
*/

/**
 * Main Processing Task - Core 0
 * Handles game modes and coordinates sensor activations
 */
void ProcessingTask(void *parameter) {
    LOG_INFO(MODULE_MAIN, "Processing task started");
    
    // Add previous game mode tracking
    static int prevGameMode = -1;  // Initialize to invalid mode
    static int prevReceivedMode = -1;  // Track raw mode from BLE
    
    for (;;) {
        // Get current game mode once per iteration
        int currentGameMode = DATA.getGameMode();
        
        // Check if we received a new mode from BLE that hasn't been processed yet
        if (currentGameMode != prevReceivedMode) {
            LOG_INFO(MODULE_MAIN, "BLE received: %s(%d) → %s(%d)",
                     getModeName(prevReceivedMode), prevReceivedMode,
                     getModeName(currentGameMode), currentGameMode);
            prevReceivedMode = currentGameMode;
            
            // Force mode change detection for BLE commands
            if (currentGameMode == TERMINATE_MODE && prevGameMode != TERMINATE_MODE) {
                LOG_INFO(MODULE_MAIN, "Terminate command received via BLE");
                prevGameMode = -1;  // Force processing by making previous mode different
            }
        }
        
        // Only process mode-specific code when the mode changes
        if (currentGameMode != prevGameMode) {
            LOG_INFO(MODULE_MAIN, "Mode transition: %s(%d) → %s(%d)",
                     getModeName(prevGameMode), prevGameMode,
                     getModeName(currentGameMode), currentGameMode);
            
            // Handle opening mode
            if (currentGameMode == OPENING_MODE) {
                LOG_INFO(MODULE_MAIN, "Entering OPENING mode");
                TOF_SENSOR.takeBaseline();
                LIGHT.turnLightON();
                // LOG_INFO(MODULE_MAIN, "Light turned on in OPENING mode");
                LIGHT.setAbleToTurnOn(false);
                DATA.setGameMode(PROCESSED_MODE);
            }
            // Handle closing mode
            else if (currentGameMode == CLOSING_MODE) {
                LIGHT.turnLightON();
                DATA.setGameMode(PROCESSED_MODE);
            }
            // Handle termination (only when first transitioning to this mode)
            else if (currentGameMode == TERMINATE_MODE) {
                LOG_INFO(MODULE_MAIN, "Entering TERMINATE mode - cleaning up resources");

                // Abort TIMED animation gracefully if it's running
                LIGHT.abortTimedAnimation();

                // Turn off light immediately
                LIGHT.turnLightOff();

                // Reset sensor flags
                if (takeMutexWithLogging(xSensorMutex, 100, MODULE_MAIN, "Sensor")) {
                    hasTOFDetectionTask = false;
                    giveMutexWithLogging(xSensorMutex, MODULE_MAIN, "Sensor");
                    LOG_DEBUG(MODULE_MAIN, "TOF detection task stopped in TERMINATE mode");

                    TOF_SENSOR.stopReading();
                    TOF_SENSOR.resetDetection();
                }

                // Send termination signal to mobile app (only if connected)
                if (BLE.getConnected()) {
                    BLE.sendMsgAndNotify("timed_terminated");
                }
            }
            
            // Update previous mode
            prevGameMode = currentGameMode;
        }
        
        // When checking manual/random mode
        if ((currentGameMode == MANUAL_MODE || currentGameMode == RANDOM_MODE ||
             currentGameMode == RHYTHM_MODE)
             && LIGHT.getAbleToTurnOn() && !LIGHT.isLightTurnedOn()) {

            // Set cooldown duration BEFORE starting TOF detection task
            if (currentGameMode == MANUAL_MODE || currentGameMode == RHYTHM_MODE) {
                // Set cooldown duration from user config (blinkBreak for MANUAL/RHYTHM)
                TOF_SENSOR.setCooldownDuration(DATA.getBlinkBreak());
                TOF_SENSOR.resetCooldown(); // Reset cooldown for MANUAL or RHYTHM mode
            } else if (currentGameMode == RANDOM_MODE) {
                // For RANDOM mode, recalibrate baseline since object position likely changed
                // This prevents false triggers from stale baseline values
                LOG_INFO(MODULE_MAIN, "Recalibrating TOF baseline for RANDOM mode");
                TOF_SENSOR.takeBaseline(false); // Don't stop reading, just recalibrate
            }

            // For RHYTHM_MODE, check sensorMode before activating sensors
            if (currentGameMode == RHYTHM_MODE) {
                int sensorMode = DATA.getSensorMode();
                LOG_INFO(MODULE_MAIN, "Rhythm Mode sensor mode: %d", sensorMode);
                
                // Only activate sensors if sensorMode is not 0
                if (sensorMode > 0) {
                    // Activate TOF sensor based on sensorMode (1 = LiDAR Only, 3 = Both)
                    if (sensorMode == 1 || sensorMode == 3) {
                        if (takeMutexWithLogging(xSensorMutex, 10, MODULE_MAIN, "Sensor")) {
                            hasTOFDetectionTask = true;
                            giveMutexWithLogging(xSensorMutex, MODULE_MAIN, "Sensor");
                            LOG_DEBUG(MODULE_MAIN, "TOF detection task requested for Rhythm Mode");
                        }
                    }
                    
                    // Activate MMWave sensor based on sensorMode (2 = MMWave Only, 3 = Both)
                    // DISABLED FOR NOW
                    /*
                    if (sensorMode == 2 || sensorMode == 3) {
                        if (takeMutexWithLogging(xMMWaveMutex, 10, MODULE_MAIN, "MMWave")) {
                            hasMMWaveDetectionTask = true;
                            giveMutexWithLogging(xMMWaveMutex, MODULE_MAIN, "MMWave");
                            LOG_DEBUG(MODULE_MAIN, "MMWave detection task requested for Rhythm Mode");
                        }
                    }
                    */
                }
            } else {
                // Original behavior for MANUAL_MODE and RANDOM_MODE
                if (takeMutexWithLogging(xSensorMutex, 10, MODULE_MAIN, "Sensor")) {
                    hasTOFDetectionTask = true;
                    giveMutexWithLogging(xSensorMutex, MODULE_MAIN, "Sensor");
                    LOG_DEBUG(MODULE_MAIN, "TOF detection task requested");
                }
            }

            LOG_INFO(MODULE_MAIN, "Turning on light in %s MODE",
                     currentGameMode == MANUAL_MODE ? "MANUAL" :
                     (currentGameMode == RHYTHM_MODE ? "RHYTHM" : "RANDOM"));
            LIGHT.turnLightON();
            LIGHT.setAbleToTurnOn(false);
            DATA.setGameMode(PROCESSED_MODE);
            prevGameMode = PROCESSED_MODE; // Update prev mode since we changed it
        }
        // Handle timed mode with TOF sensor detection
        else if (currentGameMode == TIMED_MODE && LIGHT.getAbleToTurnOn() && !LIGHT.isLightTurnedOn()) {
            // Reset cooldown to prevent false detections from stale baseline (same as RHYTHM_MODE)
            TOF_SENSOR.setCooldownDuration(DATA.getBlinkBreak());
            TOF_SENSOR.resetCooldown();

            // Activate TOF sensor for detection during TIMED mode
            if (takeMutexWithLogging(xSensorMutex, 10, MODULE_MAIN, "Sensor")) {
                hasTOFDetectionTask = true;
                giveMutexWithLogging(xSensorMutex, MODULE_MAIN, "Sensor");
                LOG_DEBUG(MODULE_MAIN, "TOF detection task requested for TIMED mode");
            }

            LOG_INFO(MODULE_MAIN, "Turning on light in TIMED MODE");
            LIGHT.turnLightON();
            LIGHT.setAbleToTurnOn(false);
            DATA.setGameMode(PROCESSED_MODE);
            prevGameMode = PROCESSED_MODE; // Update prev mode since we changed it
        }

        if (currentGameMode == CONFIG_MODE) {
            LIGHT.configNumberWipe(DATA.getConfigBlinkCount());
            BLE.sendMsgAndNotify("configDone:" + String(DATA.getConfigBlinkCount()));
            DATA.setGameMode(PROCESSED_MODE); // Auto-transition to PROCESSED_MODE after configuration
        }
        
        // Check for TOF sensor detection
        if (TOF_SENSOR.isObjectDetected()) {
            LOG_INFO(MODULE_TOF, "Object detected by TOF sensor, turning off light");
            TOF_SENSOR.resetDetection();

            // Abort TIMED animation if running
            LIGHT.abortTimedAnimation();
            LIGHT.turnLightOff();

            if (takeMutexWithLogging(xObjectDetectedMutex, 10, MODULE_TOF, "ObjectDetected")) {
                hasTOFDetectionTask = false;
                giveMutexWithLogging(xObjectDetectedMutex, MODULE_TOF, "ObjectDetected");
                LOG_DEBUG(MODULE_TOF, "Object detection handled, task flag reset");
            }
            
            // Determine which mode to report based on what was running
            // Check the actual game mode that was active before detection
            int modeBeforeDetection = DATA.getGameMode();

            // If in TIMED mode, send "timed" notification
            if (modeBeforeDetection == TIMED_MODE || modeBeforeDetection == PROCESSED_MODE && DATA.prevGameMode == TIMED_MODE) {
                LOG_INFO(MODULE_TOF, "TIMED Mode detection by LiDAR");
                handleGameMode(TIMED_MODE); // This will send "timed" notification
            }
            // Check if we're in Rhythm Mode
            else if (DATA.prevGameMode == RHYTHM_MODE) {
                LOG_INFO(MODULE_TOF, "Rhythm Mode detection by LiDAR");
                handleGameMode(RHYTHM_MODE); // This will send "rhythm" notification
            }
            else {
                handleGameMode(MANUAL_MODE);
            }
        }

        vTaskDelay(10);
    }
}

/**
 * Light Control Task - Core 0
 * Updates light behavior in a non-blocking way
 */
void LightControlTask(void *param) {
    LOG_INFO(MODULE_LIGHT, "Light control task started");
    
    for (;;) {
        LIGHT.update();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/**
 * Initialize all mutexes
 */
void initializeMutexes() {
    LOG_DEBUG(MODULE_MAIN, "Creating concurrency primitives");

    xSensorMutex = xSemaphoreCreateMutex();
    if (xSensorMutex == NULL) {
        LOG_ERROR(MODULE_MAIN, "Failed to create xSensorMutex");
    }

    // MMWave mutex disabled for now
    /*
    xMMWaveMutex = xSemaphoreCreateMutex();
    if (xMMWaveMutex == NULL) {
        LOG_ERROR(MODULE_MAIN, "Failed to create xMMWaveMutex");
    }
    */

    xObjectDetectedMutex = xSemaphoreCreateMutex();
    if (xObjectDetectedMutex == NULL) {
        LOG_ERROR(MODULE_MAIN, "Failed to create xObjectDetectedMutex");
    }

    LOG_DEBUG(MODULE_MAIN, "All concurrency primitives created successfully");
}

/**
 * Take a mutex with proper logging and error handling
 * @param mutex The mutex to take
 * @param timeout Timeout in ms
 * @param module Module name for logging
 * @param mutexName Name of mutex for logging
 * @return True if mutex was acquired, false otherwise
 */
bool takeMutexWithLogging(SemaphoreHandle_t mutex, uint32_t timeout, const char* module, const char* mutexName) {
    if (mutex == NULL) {
        // LOG_ERROR(module, "Attempted to take NULL %s mutex", mutexName);
        return false;
    }
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout)) == pdTRUE) {
        // LOG_DEBUG(module, "Acquired %s mutex", mutexName);
        return true;
    } else {
        // LOG_WARN(module, "Failed to acquire %s mutex (timeout: %lu ms)", mutexName, timeout);
        return false;
    }
}

/**
 * Give a mutex with proper logging
 * @param mutex The mutex to give
 * @param module Module name for logging
 * @param mutexName Name of mutex for logging
 */
void giveMutexWithLogging(SemaphoreHandle_t mutex, const char* module, const char* mutexName) {
    if (mutex == NULL) {
        // LOG_ERROR(module, "Attempted to give NULL %s mutex", mutexName);
        return;
    }
    
    xSemaphoreGive(mutex);
    // LOG_DEBUG(module, "Released %s mutex", mutexName);
}

/**
 * Initialize sensors with error checking
 */
bool initializeSensors() {
    // Initialize TOF sensor
    LOG_INFO(MODULE_MAIN, "Initializing TF-Luna sensor");
    if (!TOF_SENSOR.init()) {
        LOG_ERROR(MODULE_MAIN, "Failed to initialize TF-Luna sensor!");
        return false;
    }

    // Initialize MMWave radar - DISABLED FOR NOW
    /*
    LOG_INFO(MODULE_MAIN, "Initializing MMWave radar");
    if (!radar.init()) {
        LOG_ERROR(MODULE_MAIN, "Failed to initialize MMWave sensor!");
        return false;
    }
    */

    LOG_INFO(MODULE_MAIN, "All sensors initialized successfully");
    return true;
}

/**
 * Create all tasks on appropriate cores
 */
void createTasks() {
    // Sensor tasks on Core 1
    xTaskCreatePinnedToCore(TOFSensorTask, "TOFSensorTask", 6144, NULL, 2, NULL, 1);
    // MMWave task disabled for now
    // xTaskCreatePinnedToCore(MMWaveSensorTask, "MMWaveSensorTask", 4096, NULL, 2, NULL, 1);

    // Processing tasks on Core 0
    xTaskCreatePinnedToCore(ProcessingTask, "ProcessingTask", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(LightControlTask, "LightControlTask", 2048, NULL, 3, NULL, 0);
}

void setup() {
    // Initialize logging system first (replacing Serial.begin)
    Log.begin(921600);
    Log.setMinimumLogLevel(LL_DEBUG);  // Set desired minimum level
    // Log.setMinimumLogLevel(LL_INFO);

    LOG_INFO(MODULE_MAIN, "===========================================");
    LOG_INFO(MODULE_MAIN, "   %s", FIRMWARE_VERSION_STRING);
    LOG_INFO(MODULE_MAIN, "   Build: %s %s", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    LOG_INFO(MODULE_MAIN, "   Platform: ESP32");
    LOG_INFO(MODULE_MAIN, "===========================================");

    setCpuFrequencyMhz(240);
    LOG_DEBUG(MODULE_MAIN, "CPU frequency set to 240MHz");

    //POWER ENABLE PIN 5V
    pinMode(25, OUTPUT);
    digitalWrite(25, HIGH);
    delay(50);


    // Startup buzzer: turn ON for 10 seconds
    // Ensure buzzer pin is configured (LightControl constructor also sets this)
    pinMode(buzzer_pin, OUTPUT);
    LOG_INFO(MODULE_MAIN, "Startup buzzer ON for 0.4 second");
    digitalWrite(buzzer_pin, HIGH);
    delay(400);
    digitalWrite(buzzer_pin, LOW);

    // delay(10000);
    // Initialize mutexes before any other components
    initializeMutexes();
    // delay(5000);
    // Initialize components
    LOG_INFO(MODULE_MAIN, "Initializing system components");


    if (!motors.begin(19, 21, 0x20, 25)) {
        LOG_ERROR(MODULE_MAIN, "YMOTOR init failed (PCF8575 not detected)");
    } else {
        LOG_INFO(MODULE_MAIN, "YMOTOR ready");
    }


    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    pinMode(35, INPUT);

    // Initialize button pin for reset
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    lastButtonState = digitalRead(BUTTON_PIN);
    LOG_INFO(MODULE_MAIN, "Button initialized on GPIO %d", BUTTON_PIN);

    delay(50);

    
    // delay(5000);
    // Add battery status logging here
    int rawReading = BL.pinRead();
    double calibratedVoltage = BL.getVoltageFromRaw(rawReading);
    int calibratedPercentage = BL.getRawPercentage(rawReading);
    int filteredPercentage = BL.getFilteredPercentage();
    
    // Force calculation and log more details
    LOG_INFO(MODULE_MAIN, "Battery Status: %d%% (%d%% filtered) (%.2fV) Raw:%d (ADC Pin:%d)", 
             calibratedPercentage, filteredPercentage, calibratedVoltage, rawReading, BL.getAnalogPin());
    
    // Extra debug values to verify calculations
    LOG_INFO(MODULE_MAIN, "Battery DEBUG: Raw:%d = %d%% (via table lookup), %d%% (filtered)", 
             rawReading, calibratedPercentage, filteredPercentage);

    // Create tasks and finish setup
    createTasks();

    // Initialize hardware components
    LIGHT.init(true);

    // Initialize TOF sensor
    LOG_INFO(MODULE_MAIN, "Initializing TF-Luna sensor");
    // if (!TOF_SENSOR.init()) {
    //     LOG_ERROR(MODULE_MAIN, "Failed to initialize TF-Luna sensor!");
    //     LOG_ERROR(MODULE_MAIN, "Check hardware connections. Device will continue with limited functionality.");

    //     // Alert user with buzzer pattern (3 short beeps)
    //     for (int i = 0; i < 3; i++) {
    //         digitalWrite(buzzer_pin, HIGH);
    //         delay(100);
    //         digitalWrite(buzzer_pin, LOW);
    //         delay(100);
    //     }
    // } else {
    //     LOG_INFO(MODULE_MAIN, "TF-Luna sensor initialized successfully");
    // }

    BLE.init();

    LOG_INFO(MODULE_MAIN, "Setup complete");

    // Initialize sensors with error checking
    // LOG_INFO(MODULE_MAIN, "Initializing sensors");
    // if (!initializeSensors()) {
    //     LOG_ERROR(MODULE_MAIN, "Sensor initialization failed! Check connections. Button reset still available.");

    //     // Beep buzzer to alert user, but don't block
    //     for (int i = 0; i < 3; i++) {
    //         digitalWrite(buzzer_pin, HIGH);
    //         delay(200);
    //         digitalWrite(buzzer_pin, LOW);
    //         delay(200);
    //     }
    // }

    // LIGHT.configNumberWipe(1);
    delay(50);
}

void loop() {
    // Check button state for reset trigger
    int buttonReading = digitalRead(BUTTON_PIN);

    // Periodic logging of GPIO 16 and 25 state every 1 second
    unsigned long currentMillis = millis();
    #if DEBUGGER
    if (currentMillis - lastButtonLogTime >= buttonLogInterval) {
        lastButtonLogTime = currentMillis;
        LOG_INFO(MODULE_MAIN, "GPIO 16 state: %s", buttonReading == HIGH ? "HIGH" : "LOW");

        // Log GPIO 25 state
        int gpio25State = digitalRead(25);
        LOG_INFO(MODULE_MAIN, "GPIO 25 state: %s", gpio25State == HIGH ? "HIGH" : "LOW");
    }
    #endif

    // Detect state change (toggle) with simple detection
    if (buttonReading != lastButtonState) {
        // State changed
        LOG_DEBUG(MODULE_MAIN, "Button state changed from %s to %s",
                  lastButtonState == HIGH ? "HIGH" : "LOW",
                  buttonReading == HIGH ? "HIGH" : "LOW");

        lastButtonState = buttonReading;

        // Log the button state change
        LOG_INFO(MODULE_MAIN, "Button pin 16 %s detected!", buttonReading == HIGH ? "RELEASED (HIGH)" : "PRESSED (LOW)");

        // Trigger reset on any toggle (state change)
        LOG_INFO(MODULE_MAIN, "Button toggled - triggering ESP32 reset NOW!");
        delay(100); // Brief delay to ensure log is sent
        esp_restart();
    }

    // Main functionality handled by tasks
    static unsigned long lastHeartbeat = 0;
    const unsigned long heartbeatInterval = 5000; // 5 seconds

    // BLE connection timeout check - DISABLED
    /*
    static unsigned long bleStartTime = millis();
    static bool bleTimeoutChecked = false;
    const unsigned long BLE_TIMEOUT = 30000; // 10 seconds

    if (!bleTimeoutChecked && !BLE.getConnected()) {
        // Check if we've exceeded the timeout period
        if (millis() - bleStartTime > BLE_TIMEOUT) {
            LOG_WARN(MODULE_MAIN, "BLE connection timeout after %lu ms", BLE_TIMEOUT);

            // Beep four times before sleep
            for (int i = 0; i < 4; i++) {
                digitalWrite(buzzer_pin, HIGH);
                delay(200);
                digitalWrite(buzzer_pin, LOW);
                delay(200);
            }

            // 1. Set a reasonable sleep duration as backup (1 day in microseconds)
            const uint64_t ONE_DAY_US = 86400000000ULL;
            esp_sleep_enable_timer_wakeup(ONE_DAY_US);

            // 2. Keep RTC_PERIPH powered to ensure the EN pin works for reset
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
            // Turn off unnecessary domains to save power
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
            esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
            esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);

            // 3. Shut down BLE explicitly to prevent issues
            esp_bt_controller_disable();
            btStop();

            // 4. Log and delay to ensure messages are sent
            LOG_INFO(MODULE_MAIN, "Entering deep sleep - use EN pin to reset");
            delay(500);

            // 5. Enter deep sleep
            esp_deep_sleep_start();
        }
    } else if (BLE.getConnected() && !bleTimeoutChecked) {
        // Connection established before timeout
        bleTimeoutChecked = true;
        LOG_INFO(MODULE_MAIN, "BLE connected at %lu ms", millis() - bleStartTime);
    }
    */

    // Heartbeat logging at regular intervals
    #if DEBUGGER
    if (currentMillis - lastHeartbeat >= heartbeatInterval) {
        lastHeartbeat = currentMillis;
        LOG_DEBUG(MODULE_MAIN, "System heartbeat - uptime: %lu ms", currentMillis);
    }
    #endif

    delay(1000);  // Keep the loop running but don't do anything here
}
