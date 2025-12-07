# Bluetooth Communication Protocol

## Overview
This document describes the Bluetooth Low Energy (BLE) communication protocol used in the N1P project. The protocol defines how messages are structured, processed, and handled between the mobile application and the embedded device.

## Protocol Version
- Current Version: 0.0.2
- Last Updated: March 26, 2024
- Changes: Added Rhythm Mode with RGB control and timer functionality

## BLE Service and Characteristics

- **Service UUID**: `SERVICE_UUID` (defined in Global_VAR.h)
- **Main Characteristic UUID**: `CHARACTERISTIC_MSG_UUID` (for commands and notifications)
- **TX Characteristic UUID**: `CHARACTERISTIC_TX_UUID` (for notifications)
- **OTA Characteristic UUID**: `CHARACTERISTIC_OTA_UUID` (for firmware updates)

## Device Naming Convention

Devices are named using the format: `DEVICE_NAME-XXXX` where `XXXX` is derived from the ESP32's unique MAC address.

## Message Format

### Standard Command String Format
The primary message format varies by mode type but always contains 8 fields:

**Standard Modes (1-4)**:
```
gameMode,blinkBreak,timedBreak,buzzer,buzzerTime,buffer,doubleModeIndex,process
```

**Rhythm Mode (5)**:
```
5,red,green,blue,timerValue,buzzerValue,sensorMode,placeholder
```

**CONFIG_MODE (100)**:
```
100,blink_count,unused,unused,unused,unused,unused,unused
```

### CONFIG_MODE Format
The CONFIG_MODE uses a simplified format with only two parameters:
```
100,<blink_count>
```
Where:
- `100` is the mode identifier for CONFIG_MODE
- `<blink_count>` is the number of blinks to configure

### Individual Command Format
For single commands, the format is:
```
<command_type>[:<parameter>]
```

## Game Modes
The system supports several operational modes that can be triggered via Bluetooth commands:

1. `MANUAL_MODE (1)`: Manual operation mode
2. `RANDOM_MODE (2)`: Random operation mode
3. `DOUBLE_MODE (4)`: Double operation mode with index tracking
4. `TIMED_MODE (3)`: Timed operation with break periods
5. `RHYTHM_MODE (5)`: Custom RGB color control with optional timer and buzzer
6. `CONFIG_MODE (100)`: Configuration mode with blink count parameter
7. `OPENING_MODE (11)`: Initial opening sequence
8. `CLOSING_MODE (12)`: Closing sequence
9. `TERMINATE_MODE (13)`: Emergency stop/termination
10. `PROCESSED_MODE (99)`: Mode after processing completion

## Mode Descriptions and Parameters

| Mode | Command Format | Parameters | Description | System Response | Example |
|------|----------------|------------|-------------|-----------------|---------|
| **MANUAL_MODE (1)** | `1` in CSV | None | Activates manual operation mode with TOF sensor detection. | `manual` | `1,999,999,999,999,999,999,999` |
| **RANDOM_MODE (2)** | `2` in CSV | None | Activates random operation mode with TOF sensor and random timing. | `random` | `2,999,999,999,999,999,999,999` |
| **TIMED_MODE (3)** | `3` in CSV | timedBreak in param1 | Activates timed operation mode with break periods. Uses MMWave radar for detection. | `timed` | `3,5000,999,999,999,999,999,999` |
| **DOUBLE_MODE (4)** | `4` in CSV | doubleIndex in param1 | Activates double operation mode with specified index. | `double<index>` | `4,1,999,999,999,999,999,999` |
| **RHYTHM_MODE (5)** | `5` in CSV | param1: red (0-255)<br>param2: green (0-255)<br>param3: blue (0-255)<br>param4: duration (milliseconds, 0 for indefinite) | Controls RGB color sequences with timing. When duration is set to 0, the light remains on with the specified color until a new command is sent. | `rhythm` | `5,255,0,0,1000,999,999,999` |
| **MOVEMENT_MODE (6)** | `6` in CSV | None | Movement detection mode. | None | `6,999,999,999,999,999,999,999` |
| **OPENING_MODE (11)** | `11` in CSV | None | Initial opening sequence. Takes baseline TOF readings. | None | `11,999,999,999,999,999,999,999` |
| **CLOSING_MODE (12)** | `12` in CSV | None | Closing sequence. | None | `12,999,999,999,999,999,999,999` |
| **TERMINATE_MODE (13)** | `13` in CSV | None | Emergency stop/termination. | None | `13,999,999,999,999,999,999,999` |
| **RESTTIMESUP_MODE (14)** | `14` in CSV | None | Rest time is up mode. | None | `14,999,999,999,999,999,999,999` |
| **PROCESSED_MODE (99)** | `99` in CSV | None | Mode after processing completion. | None | `99,999,999,999,999,999,999,999` |
| **CONFIG_MODE (100)** | `100` in CSV | blinkCount in param1 | Configures light pattern blink count. | None | `100,4,999,999,999,999,999,999` |

## Special Case: CONFIG_MODE

The CONFIG_MODE (100) is a special case that uses a different message format than other modes:

1. **Format**: `100,<blink_count>`
2. **Example**: `100,4` configures 4 blinks
3. **Processing**: 
   ```cpp
   // In main.cpp
   case CONFIG_MODE:
       LIGHT.configNumberWipe(parameter);  // parameter is the blink count
       break;
   ```
4. **Note**: This mode does not require the full 8-parameter format and will work with just the mode (100) and blink count.

## Configuration Parameters

| Parameter | Description | Default Value | Valid Range | Notes |
|-----------|-------------|---------------|------------|-------|
| **blinkBreak** | Time between blinks in milliseconds | 500 | 100-10000 | Used in blinking patterns |
| **timedBreak** | Timeout period for timed mode in milliseconds | 500 | 1000-60000 | Controls how long timed mode remains active |
| **buzzer** | Buzzer enable flag | 1 (enabled) | 0-1 | 0=disabled, 1=enabled |
| **buzzerTime** | Buzzer duration in milliseconds | 500 | 100-2000 | How long the buzzer sounds |
| **buffer** | Buffer size/sensitivity | 500 | 1-1000 | Affects sensor sensitivity |
| **doubleModeIndex** | Index for double mode | 0 | 0-9 | Used when in DOUBLE_MODE |
| **process** | Processing flag | 0 | 0-1 | Internal use |
| **mmWaveStrength** | MMWave radar signal strength threshold | 200 | 50-500 | Lower values increase sensitivity |
| **mmWaveDistance** | MMWave radar detection distance in cm | 20 | 10-200 | Detection range |
| **mmWaveDelay** | MMWave radar processing delay | 0 | 0-1000 | Delay between readings |

## Message Structure Examples

### Standard Configuration Message
```
3,1000,5000,1,500,1,0,0
```
This sets:
- Game mode: TIMED_MODE (3)
- Blink break: 1000ms
- Timed break: 5000ms
- Buzzer: Enabled (1)
- Buzzer time: 500ms
- Buffer: 1
- Double mode index: 0
- Process: 0

### CONFIG_MODE Message
```
100,4
```
This sets:
- Game mode: CONFIG_MODE (100)
- Blink count: 4

### Simple Command
```
timed
```
This activates timed mode using default parameters.

## State Management

### Mode Transitions
1. When a mode change is requested:
   - Previous mode is tracked (`prevGameMode`)
   - New mode is validated
   - Mode-specific initialization is performed
   - Confirmation message is sent back

2. Special Mode Handling:
   ```cpp
   if (currentGameMode != prevGameMode) {
       // Mode change detected
       switch (currentGameMode) {
           case OPENING_MODE:
               // Initialize sensors
               // Turn on light
               // Set mode to PROCESSED
               break;
           case CLOSING_MODE:
               // Turn on light
               // Set mode to PROCESSED
               break;
           case CONFIG_MODE:
               // Configure light pattern with blink count
               LIGHT.configNumberWipe(blinkCount);
               break;
           case TERMINATE_MODE:
               // Emergency stop
               // Clean up resources
               break;
       }
   }
   ```

### Timed Mode Protocol
```cpp
void handleTimedMode() {
    static unsigned long lastTriggerTime = 0;
    unsigned long currentTime = millis();
    unsigned long timeout = DATA.getTimedBreak();

    if (currentTime - lastTriggerTime < timeout) {
        BLE.sendMsgAndNotify("timed");
    } else {
        BLE.sendMsgAndNotify("Timed Mode Overtimed");
    }
    lastTriggerTime = currentTime;
}
```

## Sensor Integration

### MMWave Radar Detection
1. When motion is detected in timed mode:
   - Light turns off
   - Radar stops detection
   - `timed` message is sent via BLE
   - Mode changes to PROCESSED

### TOF Sensor Detection
1. When object is detected:
   - Light turns off
   - Detection resets
   - `manual` message is sent via BLE
   - Mode changes to PROCESSED

## Connection Management

### Connection Establishment
1. Device advertises with a unique name derived from its MAC address
2. Mobile app connects to the device
3. Connection parameters are updated for optimal performance:
   ```cpp
   pServer->updateConnParams(desc->conn_handle, 12, 12, 2, 100);
   ```
4. Device LED indicates successful connection

### Disconnection Handling
1. Device detects disconnection
2. Advertising is restarted
3. Device LED indicates disconnected state

## Implementation Guidelines

### Mobile App Integration
1. **Connection Management**
   - Implement retry logic for connection failures
   - Monitor connection status
   - Handle disconnection gracefully

2. **Command Sending**
   ```javascript
   // Example format for sending commands
   sendCommand('manual');  // Switch to manual mode
   sendCommand('double:1'); // Switch to double mode with index 1
   sendCommand('timed');   // Switch to timed mode
   
   // Example format for full configuration
   sendCommand('3,1000,5000,1,500,1,0,0');  // Configure timed mode with parameters
   
   // Example for CONFIG_MODE
   sendCommand('100,4');  // Configure 4 blinks
   ```

3. **Response Handling**
   - Listen for notifications
   - Process status updates
   - Handle error conditions

### Embedded Device Implementation
1. **Command Processing**
   ```cpp
   // Example command processing
   if (command == "manual") {
       DATA.setGameMode(MANUAL_MODE);
   } else if (command.startsWith("double")) {
       int index = command.substring(6).toInt();
       DATA.setGameMode(DOUBLE_MODE);
       DATA.setDoubleModeIndex(index);
   } else if (command.startsWith("100,")) {
       // Special handling for CONFIG_MODE
       int blinkCount = command.substring(4).toInt();
       LIGHT.configNumberWipe(blinkCount);
   }
   ```

2. **Status Updates**
   ```cpp
   // Example status notification
   BLE.sendMsgAndNotify("timed");  // Send timed mode confirmation
   ```

## Security Considerations

1. **Data Validation**
   - All incoming commands must be validated
   - Parameters must be within acceptable ranges
   - Invalid commands should be rejected

2. **Connection Security**
   - Implement BLE security features
   - Consider encryption for sensitive commands
   - Monitor for unauthorized connection attempts

## Debugging

### Logging
The system implements comprehensive logging:
```cpp
LOG_INFO(MODULE_MAIN, "Game mode changed: %d -> %d", prevGameMode, currentGameMode);
LOG_DEBUG(MODULE_MMWAVE, "Task heartbeat: Frames:%lu Empty:%lu Sync:%lu Bytes:%d");
LOG_WARN(MODULE_MMWAVE, "Buffer overflow detected");
```

### Performance Monitoring
- Task execution times are tracked
- Buffer states are monitored
- System heartbeat provides regular status updates

## Future Enhancements

1. **Protocol Extensions**
   - Additional command types for new features
   - Enhanced error reporting
   - Extended status information

2. **Security Improvements**
   - Command authentication
   - Encrypted communication
   - Connection validation

3. **Performance Optimizations**
   - Command batching
   - Reduced notification frequency
   - Optimized state transitions

## Troubleshooting

### Common Issues

1. **Connection Failures**
   - Check device power
   - Verify BLE service availability
   - Confirm correct pin configuration

2. **Command Timeouts**
   - Check for buffer overflows
   - Verify command format
   - Monitor system load

3. **Mode Transition Failures**
   - Check current system state
   - Verify sensor initialization
   - Monitor task execution

### Resolution Steps

1. **System Reset Protocol**
   - Send terminate command
   - Wait for cleanup completion
   - Reinitialize system
   - Reestablish connection

2. **Error Recovery**
   - Log error conditions
   - Reset affected subsystems
   - Restore known good state
   - Resume normal operation

### Rhythm Mode Protocol

The RHYTHM_MODE (5) uses the standard 8-parameter message format:

1. **Format**: `5,red,green,blue,timerValue,buzzerValue,sensorMode,placeholder`
2. **Parameters**:
   - `red`: Red color value (0-255)
   - `green`: Green color value (0-255)
   - `blue`: Blue color value (0-255)
   - `timerValue`: Auto-off timer in milliseconds
     - 0: Light stays on until sensor detection occurs
     - >0: Light turns off after specified duration
   - `buzzerValue`: Buzzer duration in milliseconds (0 to disable)
   - `sensorMode`: Sensor selection (0-3)
     - 0: No sensor detection (light stays on until timer expires)
     - 1: LiDAR Only
     - 2: MMWave Only
     - 3: Both sensors
   - Last parameter: Set to 999 (unused)

#### Behavior
- **Color Control**: Sets LED color using RGB values
- **Timer Function**: 
  - When timerValue = 0: Light remains on until sensor detection occurs
  - When timerValue > 0: Light automatically turns off after specified duration
- **Buzzer Function**: When buzzerValue > 0:
  - Activates buzzer for specified duration independently of timer
- **Sensor Detection**: Based on sensorMode value:
  - Mode 0: No detection, timer-only operation
  - Mode 1: LiDAR detection only
  - Mode 2: MMWave detection only
  - Mode 3: Both sensors active
- **Response**: Returns "rhythm" on successful activation

#### Examples

| Command | Effect |
|---------|--------|
| `5,255,0,0,0,500,1,999` | Red light, stays on until LiDAR detection, 0.5s buzzer |
| `5,0,255,0,3000,0,0,999` | Green light, 3s timer, no sensors |
| `5,0,0,255,0,0,2,999` | Blue light, stays on until MMWave detection |
| `5,255,255,0,0,1000,3,999` | Yellow light, stays on until any sensor detection, 1s buzzer |

## Additional Notes

- The RHYTHM_MODE (5) uses a different message format than other modes, and the format is not documented in the original file. The implementation assumes that the format is `5,<num_colors>` followed by color data.
- The original file does not mention the RHYTHM_MODE (5) in the mode descriptions and parameters table. The implementation assumes that the mode is included in the table.
- The original file does not mention the RHYTHM_MODE (5) in the mode descriptions and parameters table. The implementation assumes that the mode is included in the table.

### Special Case: RHYTHM_MODE Multiple Colors
For sequences with multiple colors, send consecutive messages: 

#### Field Descriptions

| Field | Description | Range | Notes |
|-------|-------------|-------|-------|
| 1 | Game Mode (5) | 5 | Identifies Rhythm Mode |
| 2 | Red Value | 0-255 | Red component of RGB |
| 3 | Green Value | 0-255 | Green component of RGB |
| 4 | Blue Value | 0-255 | Blue component of RGB |
| 5 | Timer Value | 0-60000 | Buzzer & auto-off duration (0 = disabled) |
| 6-8 | Placeholders | 999 | Reserved for future use |

#### Behavior
- **Color Control**: Sets LED color using RGB values
- **Timer Function**: When timerValue > 0:
  - Activates buzzer for specified duration
- **LiDAR Detection**: Remains active as in Manual Mode
- **Response**: Returns "rhythm" on successful activation

#### Examples

| Command | Effect |
|---------|--------|
| `5,255,0,0,1000,999,999,999` | Red light, 1s timer |
| `5,0,255,0,0,999,999,999` | Green light, no timer |
| `5,0,0,255,5000,999,999,999` | Blue light, 5s timer |
| `5,255,255,0,500,999,999,999` | Yellow light, 0.5s timer |

## Implementation Notes

### Rhythm Mode Implementation
```javascript
// Example: Setting red color with 1-second timer
sendCommand('5,255,0,0,1000,999,999,999');

// Example: Setting green color with no timer
sendCommand('5,0,255,0,0,999,999,999');
```

[Rest of the document remains unchanged]