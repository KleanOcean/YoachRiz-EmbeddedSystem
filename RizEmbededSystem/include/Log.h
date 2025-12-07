/**
 * @file Log.h
 * @brief Standardized logging system for N1P
 */
#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

// Log levels
enum LogLevel {
    LL_DEBUG = 0,   // Detailed debugging information
    LL_INFO = 1,    // Normal operation information 
    LL_WARN = 2,    // Warning conditions
    LL_ERROR = 3,    // Error conditions
    LL_NONE = 4      // No logging
};

// Standard module names
#define MODULE_MAIN "MAIN"
#define MODULE_TOF "TOF"
#define MODULE_MMWAVE "MMWAVE"
#define MODULE_LIGHT "LIGHT"
#define MODULE_BLE "BLE"
#define MODULE_DATA "DATA"

class LogClass {
private:
    LogLevel _minLogLevel = LL_INFO;  // Default level
    bool _serialEnabled = true;              // Default enabled
    bool _initialized = false;               // Initialization flag
    
public:
    /**
     * Initialize the logging system
     * @param baudRate The baud rate for Serial communication
     */
    void begin(unsigned long baudRate = 115200) {
        if (!_initialized) {
            Serial.begin(baudRate);
            _initialized = true;
            log(MODULE_MAIN, LL_INFO, "Logging system initialized");
        }
    }
    
    /**
     * Set the minimum log level to display
     * @param level Minimum level (messages below this level will be filtered)
     */
    void setMinimumLogLevel(LogLevel level) {
        _minLogLevel = level;
        log(MODULE_MAIN, LL_INFO, "Log level set to %d", level);
    }
    
    /**
     * Enable or disable serial logging
     * @param enable True to enable, false to disable
     */
    void enableSerialLogging(bool enable) {
        _serialEnabled = enable;
        log(MODULE_MAIN, LL_INFO, "Serial logging %s", enable ? "enabled" : "disabled");
    }
    
    /**
     * Get string representation of log level
     * @param level Log level enum value
     * @return String representation
     */
    const char* getLevelName(LogLevel level) {
        switch (level) {
            case LL_DEBUG: return "DEBUG";
            case LL_INFO:  return "INFO";
            case LL_WARN:  return "WARN";
            case LL_ERROR: return "ERROR";
            default:              return "UNKNOWN";
        }
    }
    
    /**
     * Log a message with the given level and module
     * @param module Module name
     * @param level Log level
     * @param format Format string (printf-style)
     * @param ... Variable arguments
     */
    void log(const char* module, LogLevel level, const char* format, ...) {
        // Skip if level is below minimum or logging is disabled
        if (level < _minLogLevel || !_serialEnabled) {
            return;
        }
        
        // Format timestamp
        char timestamp[16];
        snprintf(timestamp, sizeof(timestamp), "[%8lu ms]", millis());
        
        // Format module and level
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "%s[%s][%s] ", 
                 timestamp, module, getLevelName(level));
        
        // Print prefix
        Serial.print(prefix);
        
        // Format and print the actual message
        va_list args;
        va_start(args, format);
        
        // Calculate the required buffer size
        va_list args_copy;
        va_copy(args_copy, args);
        int len = vsnprintf(NULL, 0, format, args_copy) + 1;
        va_end(args_copy);
        
        // Dynamically allocate the buffer
        char* buffer = new char[len];
        vsnprintf(buffer, len, format, args);
        
        // Print the message
        Serial.println(buffer);
        
        // Clean up
        delete[] buffer;
        va_end(args);
    }
};

// Global Log instance
extern LogClass Log;

// Convenience macros
#define LOG_DEBUG(module, format, ...) Log.log(module, LL_DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(module, format, ...)  Log.log(module, LL_INFO,  format, ##__VA_ARGS__)
#define LOG_WARN(module, format, ...)  Log.log(module, LL_WARN,  format, ##__VA_ARGS__)
#define LOG_ERROR(module, format, ...) Log.log(module, LL_ERROR, format, ##__VA_ARGS__)

#endif // LOG_H 