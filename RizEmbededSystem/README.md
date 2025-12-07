# Yoach 1 - ESP32 Firmware Technical Details

**Current Version:** v0.0.2
**Last Updated:** 2025-12-06
**Platform:** ESP32 (ESP-IDF / Arduino Framework)

This project implements the firmware for the **Yoach 1** device using the ESP32. It integrates multiple subsystems including:
- **Bluetooth Control** (with OTA updates),
- **Light Control**,
- **Data Control** for game mode settings,
- **Sensor Modules** (Time-of-Flight sensor and mmWave sensor),
- **Battery Monitoring** and **Task Management** using FreeRTOS.

Below is the system architecture overview for Yoach 1:


```mermaid

flowchart TD
  subgraph ESP32_Project["Yoach 1 - ESP32 Firmware"]
    direction TB

    %% Main Entry Point
    A1[Main Entry - main.cpp] -->|Initializes| B1(BluetoothControl)
    A1 -->|Initializes| B2(LightControl)
    A1 -->|Initializes| B3(DataControl)
    A1 -->|Initializes| B4(Sensor Modules)
    A1 -->|Initializes| B7(Battery Monitoring)
    A1 -->|Starts| B5(FreeRTOS Tasks)

    %% Bluetooth Control
    subgraph B1["BluetoothControl"]
      B1_1[BLE Server NimBLE]
      B1_2[Message Handling Callback]
      B1_3[OTA Update Characteristic]
      B1_4[Notifications to App]
    end

    %% Light Control
    subgraph B2["LightControl"]
      B2_1[RGB LED Strip - NeoPixel]
      B2_2[Buzzer Control]
      B2_3[Light Effects Engine]
      B2_4[Brightness PID Control LightPid]
    end

    %% Data Control
    subgraph B3["DataControl"]
      B3_1[Game Mode & Settings]
      B3_2[BLE Message Parsing]
      B3_3[State Management]
      B3_4[Sensor Configurations]
    end

    %% Sensor Modules
    subgraph B4["Sensor Modules"]
      B4_1[TF-Luna - ToF Sensor]
      B4_2[MMWave - Radar Sensor]
      B4_3[Sensor State Management]
    end

    %% OTA Updates
    subgraph B6["OTA Updates"]
      B6_1[BLE OTA Handler otaCallback]
      B6_2[Firmware Update Process esp_ota]
      B6_3[Restart & Bootloader Update]
    end

    %% Battery Monitoring
    subgraph B7["Battery Monitoring Pangodream_18650_CL"]
      B7_1[Read ADC Voltage]
      B7_2[Voltage-to-Percentage Conversion]
      B7_3[Filtered Percentage Reporting]
    end

    %% Task Management (FreeRTOS)
    subgraph B5["FreeRTOS Tasks"]
      B5_1[ProcessingTask - Main Logic]
      B5_2[TOFSensorTask - TOF Readings]
      B5_3[MMWaveSensorTask - Radar Readings]
      B5_4[LightControlTask - Effects Update]
    end

    %% Logging System
    subgraph B8["Logging System Log.h"]
      B8_1[Serial Output]
      B8_2[Formatted Logs]
      B8_3[Log Levels DEBUG, INFO, WARN, ERROR]
    end

    %% Connections
    B1 -->|BLE Commands| B3
    B3 -->|Game Settings| B2
    B3 -->|Sensor Config| B4
    B4 -->|Sensor Data| B5_1
    B5_1 -->|Triggers Effects| B2
    B5_1 -->|Sends Updates| B1_4
    B1_3 -->|Firmware Data| B6_1
    B6 -->|OTA Success| A1
    B7 -->|Battery Status| B1_4
    B1 -->|Logs| B8
    B2 -->|Logs| B8
    B3 -->|Logs| B8
    B4 -->|Logs| B8
    B6 -->|Logs| B8
    B7 -->|Logs| B8
  end

```

---

## Introduction

This firmware is built for the **Yoach 1** device, providing robust control and sensor processing for interactive use. It leverages the ESP32's dual-core capabilities to run tasks concurrently:
- **Core 0** mainly handles high-level logic (e.g., processing sensor events and controlling game modes).
- **Core 1** is dedicated to time-critical sensor tasks, including processing data from the TF-Luna TOF sensor and Radar sensor.

---

## Getting Started

### Prerequisites
- **Hardware:** ESP32 development board, TF-Luna TOF sensor (or equivalent), LED components, and any other sensor modules.
- **Software:** 
  - [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) or the Arduino IDE set up for ESP32 development.
  - Required libraries for BLE, FreeRTOS, sensor interfacing, etc.

### Hardware Setup
- **I2C Connections:**  
  Check [`include/Global_VAR.h`](include/Global_VAR.h) for the I2C pin definitions.  
  > **Note:** These pins can be reconfigured for other protocols (like TX/RX) if necessary.
- **Sensor Wiring:**  
  Connect the TF-Luna sensor via I2C and ensure proper power conditioning.

### Software Setup
1. **Clone the Repository:**
   ```bash
   git clone <repository-url>
   cd <repository-directory>
   ```
2. **Configure and Build:**
   - **Using ESP-IDF:**
     ```bash
     idf.py build
     idf.py flash
     idf.py monitor
     ```
   - **Using Arduino IDE:**  
     Open the project, select the correct ESP32 board, and then compile/upload.

### Running and Testing
- Once flashed, the firmware initializes and configures all subsystems.
- **Game Modes:**  
  The firmware supports multiple game modes. In **MANUAL_MODE**, the TF-Luna sensor takes a baseline first, then the LED lights up, and object detection begins.
- Use a BLE client or Serial Monitor to view logs and interact with the device.

---

## Project Structure

- **src/**: Contains:
  - `main.cpp`: Main entry, task creation, and system initialization.
  - Sensor and control tasks.
- **include/**: Contains header files including global variable definitions.
- **README.md:** This document provides an overview and starting guide.

---

## Detailed Documentation

For more in-depth information on specific aspects of the project, please refer to the documentation within the `docs/` directory. Key documents include:

-   **[`docs/README.md`](docs/README.md)**: Provides an overview and links to all other documentation files.
-   [`docs/SYSTEM_ARCHITECTURE.md`](docs/SYSTEM_ARCHITECTURE.md): High-level system design.
-   [`docs/FUNCTIONAL_DESCRIPTION.md`](docs/FUNCTIONAL_DESCRIPTION.md): Description of user modes and features.
-   [`docs/MODULE_REFERENCE.md`](docs/MODULE_REFERENCE.md): Technical details on software modules.
-   [`docs/HARDWARE_INTERFACE.md`](docs/HARDWARE_INTERFACE.md): Pinouts and hardware specifics.
-   [`docs/BLE_API_REFERENCE.md`](docs/BLE_API_REFERENCE.md): BLE communication protocol.
-   [`docs/SYSTEM_PROTOCOL.md`](docs/SYSTEM_PROTOCOL.md): Communication patterns, feedback (BLE/logs), and terminology.
-   [`docs/PRD_Window_Mode.md`](docs/PRD_Window_Mode.md): Product Requirements Document for Window Mode (upcoming feature).

---

## Version History

### v0.0.2 (2025-12-06)
- **Window Mode PRD**: Added comprehensive Product Requirements Document for Window Mode training
- **iPad Integration**: Optimized UI layout and spacing for iPad mini display
- **Config View**: Converted sliders to horizontal layout for better iPad experience
- **Documentation**: Enhanced project documentation structure

### v0.0.1 (Initial Release)
- Core firmware implementation with multi-mode support
- BLE communication and OTA updates
- TOF and MMWave sensor integration
- RGB LED control with multiple light effects
- FreeRTOS task management
- Game modes: Manual, Random, Timed, Double, Rhythm, Movement

---

## Contributing

Feel free to fork and open issues or pull requests if you have improvements or fixes.

---

Happy Coding!