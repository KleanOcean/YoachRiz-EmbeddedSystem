# Yoach 1 Firmware Upgrade Plan - v0.1.0

**Objective:** Refactor the Yoach 1 firmware to implement a more robust, maintainable, and efficient hybrid architecture, leveraging Layered Design, Event-Driven Communication via FreeRTOS primitives, a Finite State Machine (FSM) for core logic, and OOP principles.

**Target Version:** 0.1.0

---

## 1. Current State Analysis (v0.0.2 limitations)

The current firmware (v0.0.2) successfully integrates multiple components but exhibits some architectural weaknesses that hinder scalability and maintainability:

1.  **Complex `ProcessingTask`:** The core logic in `ProcessingTask` relies heavily on `if/else if` chains and `prevGameMode` checks to manage state transitions and mode-specific actions. This becomes difficult to manage and extend as more modes or features are added.
2.  **Polling/Flag-Based Task Signaling:** Communication *between* `ProcessingTask` and sensor tasks (`TOFSensorTask`, `MMWaveSensorTask`) primarily uses shared boolean flags (`hasTOFDetectionTask`, `hasMMWaveDetectionTask`) protected by mutexes (`xSensorMutex`, `xMMWaveMutex`).
    *   This forces sensor tasks to periodically wake up, acquire a mutex, check a flag, and potentially go back to sleep if the flag isn't set, which is inefficient (busy-waiting characteristic).
    *   `ProcessingTask` needs to acquire mutexes just to set flags.
    *   Mutexes are used for *signaling*, which is not their primary purpose (they are for protecting shared *data* access).
3.  **Direct Method Calls for Effects:** `ProcessingTask` directly calls `LightControl` methods (e.g., `LIGHT.turnLightON()`). If these methods involve delays or complex sequences, they could block `ProcessingTask`.
4.  **`loop()` Function Logic:** The main Arduino `loop()` function still contains logic (BLE timeout check, deep sleep). In an RTOS environment, this logic should ideally reside within a dedicated low-priority task.

---

## 2. Proposed Architecture (v0.1.0)

The v0.1.0 architecture will adhere to the hybrid model discussed previously:

### 2.1 Guiding Principles:

*   **Layered Structure:** Maintain clear separation between Application, Services/Middleware, Drivers, and HAL.
*   **Event-Driven Communication:** Replace flag-based signaling with non-blocking FreeRTOS primitives (Queues and Event Groups).
*   **Finite State Machine (FSM):** Implement the core mode logic within `ProcessingTask` as a well-defined FSM.
*   **Object-Oriented Programming (OOP):** Continue using classes for drivers and services to ensure encapsulation and modularity.

### 2.2 Layered Structure Overview:

*   **Application Layer:** Contains Tasks (`ProcessingTask` w/ FSM, `LightControlTask`, `TOFSensorTask`, `MMWaveSensorTask`, `SystemMonitorTask`) responsible for specific application behaviors.
*   **Services/Middleware Layer:** Provides core services (`DataControl`, `BluetoothControl`, `Log`, `OTA`) and the RTOS (`FreeRTOS Queues/Events/Mutexes`).
*   **Driver Layer:** Contains OOP classes for controlling hardware (`LightControl`, `TF_Luna_UART`, `MMWave`, `Pangodream_18650_CL`).
*   **HAL Layer:** Platform-provided hardware abstractions (ESP-IDF/Arduino Core).

### 2.3 Event-Driven Communication Details:

We will introduce the following FreeRTOS primitives:

1.  **`cmdQueue` (QueueHandle_t):**
    *   **Purpose:** Passes commands received via BLE from `BluetoothControl` callbacks to `ProcessingTask`.
    *   **Sender:** `BluetoothControl` (callback context).
    *   **Receiver:** `ProcessingTask`.
    *   **Data:** Struct containing command type and parameters (parsed from BLE string).
2.  **`sensorControlEventGroup` (EventGroupHandle_t):**
    *   **Purpose:** Signals sensor tasks (`TOFSensorTask`, `MMWaveSensorTask`) to start or stop operations. Replaces `has...Task` flags and associated mutexes (`xSensorMutex`, `xMMWaveMutex`).
    *   **Setter:** `ProcessingTask`.
    *   **Waiter:** `TOFSensorTask`, `MMWaveSensorTask`.
    *   **Bits:**
        *   `BIT_START_TOF`
        *   `BIT_STOP_TOF`
        *   `BIT_START_MMWAVE`
        *   `BIT_STOP_MMWAVE`
3.  **`sensorResultEventGroup` (EventGroupHandle_t):**
    *   **Purpose:** Signals `ProcessingTask` that a sensor has completed an action or detected something significant.
    *   **Setter:** `TOFSensorTask`, `MMWaveSensorTask`.
    *   **Waiter:** `ProcessingTask`.
    *   **Bits:**
        *   `BIT_TOF_DETECTED`
        *   `BIT_TOF_CYCLE_COMPLETE` (Optional: if needed beyond just detection)
        *   `BIT_MMWAVE_DETECTED`
        *   `BIT_MMWAVE_CYCLE_COMPLETE` (Optional)
    *   *Alternative:* Could use Queues if detailed sensor data needs to be passed *immediately* upon detection. For now, Event Group signals detection, and `ProcessingTask` can fetch details from sensor objects if needed (protecting with mutex ONLY if concurrent access is possible).
4.  **`lightEffectQueue` (QueueHandle_t):**
    *   **Purpose:** Sends requests for light/buzzer effects from `ProcessingTask` to `LightControlTask`. Replaces direct calls from `ProcessingTask`.
    *   **Sender:** `ProcessingTask`.
    *   **Receiver:** `LightControlTask`.
    *   **Data:** Struct defining the requested effect (e.g., effect type enum, color, duration, parameters).

### 2.4 FSM for `ProcessingTask`:

*   **Structure:** Define `enum State { STATE_IDLE, STATE_OPENING, STATE_MANUAL, STATE_RANDOM, STATE_TIMED, STATE_DOUBLE, STATE_RHYTHM, STATE_CONFIG, STATE_CLOSING, STATE_TERMINATING, ... };`
*   Define handler functions: `void handle_state_idle()`, `void handle_state_manual()`, etc.
*   Define transition functions: `void enter_state_manual()`, `void exit_state_manual()`, etc.
*   `ProcessingTask` Loop:
    1.  Wait indefinitely (`portMAX_DELAY`) on `cmdQueue` and `sensorResultEventGroup` using `xQueueReceive` and `xEventGroupWaitBits`.
    2.  If a command is received: Parse it (using `DataControl`), determine the target state, call `exit_current_state()`, update state variable, call `enter_new_state()`.
    3.  If a sensor event is received: Call the current state's handler function (`handle_current_state()`) which will process the sensor event bit and potentially trigger state transitions or actions (like sending light effects).
    4.  The `handle_state_xxx()` functions contain the logic previously executed *within* a specific mode (outside the `if (mode != prevMode)` block).
    5.  `enter_state_xxx()` contains logic previously run *only on entering* a mode.
    6.  `exit_state_xxx()` contains logic needed *only when leaving* a mode.

### 2.5 Refined Task Responsibilities:

*   **`ProcessingTask`:** Focuses *only* on state management (FSM), command interpretation, and coordinating other tasks via events/queues. Does *not* perform direct hardware I/O, sensor reading, or blocking delays for effects.
*   **`LightControlTask`:** Becomes purely event-driven, waiting on `lightEffectQueue`. Executes non-blocking effects using the `LightControl` driver methods.
*   **`TOFSensorTask` / `MMWaveSensorTask`:** Become purely event-driven. Wait for `BIT_START_xxx` in `sensorControlEventGroup`. When started, enter a loop reading the sensor via driver methods. If detection occurs, set `BIT_xxx_DETECTED` in `sensorResultEventGroup`. Check for `BIT_STOP_xxx` on each loop iteration or use `xEventGroupWaitBits` with a timeout to be responsive to stop commands. When stopped, return to waiting for `BIT_START_xxx`.
*   **`SystemMonitorTask` (New):** A low-priority task responsible for periodic checks like:
    *   BLE connection timeout and deep sleep logic (moved from `loop()`).
    *   System heartbeat logging.
    *   Periodic battery level checks (calling `BL.getFilteredPercentage()`).
    *   Potentially other background monitoring.
*   **`loop()`:** Becomes empty or contains only `vTaskDelay(portMAX_DELAY)` as all logic moves to tasks.

---


## 3. Refactoring Checklist - v0.1.0

**Phase 1: Setup RTOS Communication & Background Task**

*   **Step 1: Setup RTOS Primitives (in `main.cpp`)**
    *   `[ ]` **Headers:** Include `freertos/FreeRTOS.h`, `freertos/task.h`, `freertos/queue.h`, `freertos/event_groups.h`.
    *   `[ ]` **Global Handles:** Declare:
        *   `QueueHandle_t cmdQueue;`
        *   `EventGroupHandle_t sensorControlEventGroup;`
        *   `EventGroupHandle_t sensorResultEventGroup;`
        *   `QueueHandle_t lightEffectQueue;`
    *   `[ ]` **Event Group Bits:** Define bits (e.g., `#define BIT_START_TOF (1 << 0)`):
        *   `BIT_START_TOF`, `BIT_STOP_TOF`
        *   `BIT_START_MMWAVE`, `BIT_STOP_MMWAVE`
        *   `BIT_TOF_DETECTED`, `BIT_MMWAVE_DETECTED`
        *   *(Optional: `BIT_TOF_ERROR`, `BIT_MMWAVE_ERROR`)*
    *   `[ ]` **Queue Structs:** Define data structures:
        *   `BleCommand_t` for `cmdQueue` (e.g., `enum CommandType cmdType; int params[MAX_PARAMS];` or similar).
        *   `LightEffectRequest_t` for `lightEffectQueue` (e.g., `enum EffectType effect; int color[3]; uint32_t duration;` etc.).
    *   `[ ]` **Creation:** In `setup()` or `initializeRTOSObjects()` function:
        *   `cmdQueue = xQueueCreate(CMD_QUEUE_LENGTH, sizeof(BleCommand_t));` (Choose appropriate length, e.g., 5-10).
        *   `sensorControlEventGroup = xEventGroupCreate();`
        *   `sensorResultEventGroup = xEventGroupCreate();`
        *   `lightEffectQueue = xQueueCreate(LIGHT_QUEUE_LENGTH, sizeof(LightEffectRequest_t));` (Choose appropriate length, e.g., 5-10).
    *   `[ ]` **Error Checking:** Add `if (handle == NULL)` checks after each creation, log errors if creation fails.

*   **Step 2: Implement `SystemMonitorTask`**
    *   `[ ]` **Prototype:** `void SystemMonitorTask(void *parameter);`
    *   `[ ]` **Implementation:** Create `SystemMonitorTask` function with an infinite `for(;;)` loop.
    *   `[ ]` **Move Logic:** Transfer BLE timeout/deep sleep logic from `loop()` into the task loop.
    *   `[ ]` **Move Logic:** Transfer heartbeat logging from `loop()` into the task loop.
    *   `[ ]` **Add Battery Check:** Add periodic `BL.getFilteredPercentage()` call and potential logging/notification if needed.
    *   `[ ]` **Add Delay:** Ensure a `vTaskDelay(pdMS_TO_TICKS(desired_interval));` (e.g., 1000-5000ms) exists within the loop.
    *   `[ ]` **Task Creation:** In `setup()`, call `xTaskCreatePinnedToCore(SystemMonitorTask, "SysMonTask", SYS_MON_STACK_SIZE, NULL, 1, NULL, 0);` (Core 0, priority 1 is typical for low-priority tasks). Define `SYS_MON_STACK_SIZE` appropriately (e.g., 2048).
    *   `[ ]` **Cleanup `loop()`:** Delete the content of `loop()` or leave only `vTaskDelay(portMAX_DELAY);`.

**Phase 2: Refactor Peripheral Tasks**

*   **Step 3: Refactor Sensor Tasks (`TOFSensorTask`, `MMWaveSensorTask`)**
    *   *(Apply to both `TOFSensorTask` and `MMWaveSensorTask`)*
    *   `[ ]` **Remove Globals:** Delete `hasTOFDetectionTask`/`hasMMWaveDetectionTask` variables.
    *   `[ ]` **Remove Mutexes (Signaling):** Remove `xSemaphoreTake`/`Give` calls around the old flag checks (`xSensorMutex`, `xMMWaveMutex`). Retain mutexes *only* if protecting shared *data* (evaluate necessity later).
    *   `[ ]` **Wait for Start:** Modify the task's outer loop:
        ```c
        for (;;) {
            EventBits_t uxBits = xEventGroupWaitBits(
                sensorControlEventGroup,  // Event group to wait on
                BIT_START_xxx,            // Bit(s) to wait for
                pdTRUE,                   // Clear the bit(s) on exit
                pdFALSE,                  // Wait for ANY specified bit (doesn't matter for one bit)
                portMAX_DELAY);           // Wait indefinitely

            if (uxBits & BIT_START_xxx) {
                LOG_INFO(MODULE_xxx, "Start event received.");
                // Sensor reading loop starts here...
            }
        }
        ```
    *   `[ ]` **Sensor Reading Loop:** Inside the `if (uxBits & BIT_START_xxx)` block:
        ```c
        while (true) { // Loop while active
            // Perform sensor read using driver
            // e.g., bool detected = TOF_SENSOR.updateLidarDataAndCheckDetection();
            // e.g., bool detected = radar.isObjectInRange();

            if (detected) {
                LOG_INFO(MODULE_xxx, "Detection occurred!");
                xEventGroupSetBits(sensorResultEventGroup, BIT_xxx_DETECTED);
                // Decide if detection should also trigger stopping (break;)
                // or if it should continue running until explicitly stopped.
            }

            // Check for stop command without blocking
            EventBits_t controlBits = xEventGroupGetBits(sensorControlEventGroup);
            if (controlBits & BIT_STOP_xxx) {
                LOG_INFO(MODULE_xxx, "Stop event received.");
                xEventGroupClearBits(sensorControlEventGroup, BIT_STOP_xxx); // Clear the stop bit
                break; // Exit the inner sensor reading loop
            }

            vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL)); // Small delay between reads
        } // End of sensor reading loop
        LOG_INFO(MODULE_xxx, "Sensor loop stopped. Waiting for start event.");
        // Task will loop back to xEventGroupWaitBits for BIT_START_xxx
        ```
    *   `[ ]` **Remove Old Code:** Delete any code that manually reset the old `has...Task` flag.

*   **Step 4: Refactor `LightControlTask`**
    *   `[ ]` **Wait on Queue:** Modify task loop:
        ```c
        LightEffectRequest_t effectRequest;
        for (;;) {
            if (xQueueReceive(lightEffectQueue, &effectRequest, portMAX_DELAY) == pdPASS) {
                LOG_DEBUG(MODULE_LIGHT, "Received effect request: %d", effectRequest.effect);
                // Process the request
                switch (effectRequest.effect) {
                    case EFFECT_TYPE_WIPE:
                        // Call non-blocking LIGHT.startWipe(effectRequest.color, ...);
                        break;
                    case EFFECT_TYPE_BUZZER:
                        // Call non-blocking LIGHT.playBuzzerAsync(effectRequest.duration);
                        break;
                    // Add other effect types
                    default:
                        LOG_WARN(MODULE_LIGHT, "Unknown effect type: %d", effectRequest.effect);
                }
            }
        }
        ```
    *   `[ ]` **Ensure Driver Non-Blocking:** Review/Refactor `LightControl` methods called by this task. If a method performs a long animation with internal `delay()` calls, refactor it:
        *   Option A: The method only *starts* the effect and uses timers/internal state managed by `LightControl::update()` called rapidly *by* `LightControlTask`.
        *   Option B: `LightControlTask` itself breaks down the effect into steps, calling simple `LightControl` methods (e.g., `setPixel`, `show`) with `vTaskDelay` *in between steps within the task*.
    *   `[ ]` **Remove Old Logic:** Delete any logic not driven by `lightEffectQueue`.

**Phase 3: Refactor Core Logic (`ProcessingTask` FSM)**

*   **Step 5: Refactor `ProcessingTask` (FSM Implementation)**
    *   `[ ]` **FSM Definitions:** Define `enum State`, `StateHandler`, `StateTransitionHandler`.
    *   `[ ]` **Prototypes:** Declare all `handle_state_xxx()`, `enter_state_xxx()`, `exit_state_xxx()` prototypes.
    *   `[ ]` **State Variables:** Declare `static State currentState`, `static StateHandler currentHandler`, `static StateTransitionHandler currentExitHandler`. Initialize to starting state (e.g., `STATE_OPENING`).
    *   `[ ]` **Main Loop Structure:** Implement the core loop:
        ```c
        BleCommand_t receivedCommand;
        EventBits_t receivedEventBits;
        const TickType_t xMaxBlockTime = pdMS_TO_TICKS(50); // Example: Check periodically or use portMAX_DELAY if only events/cmds trigger actions

        for (;;) {
            bool commandReceived = (xQueueReceive(cmdQueue, &receivedCommand, 0) == pdPASS); // Check queue without blocking much
            receivedEventBits = xEventGroupWaitBits(sensorResultEventGroup,
                                                    BIT_TOF_DETECTED | BIT_MMWAVE_DETECTED, // Bits to wait for
                                                    pdTRUE,          // Clear bits on exit
                                                    pdFALSE,         // Wait for ANY bit
                                                    commandReceived ? 0 : xMaxBlockTime); // Block if no command pending

            if (commandReceived) {
                LOG_INFO(MODULE_MAIN, "Command received: %d", receivedCommand.cmdType);
                // Parse command using DataControl (might update DATA state directly)
                State nextState = determineNextState(currentState, receivedCommand); // Needs implementation

                if (nextState != currentState) {
                    LOG_INFO(MODULE_MAIN, "State Transition: %d -> %d", currentState, nextState);
                    if (currentExitHandler != NULL) currentExitHandler();

                    // Update state variables based on nextState
                    currentState = nextState;
                    // Assign new currentHandler and currentExitHandler based on nextState
                    // Get new enterHandler based on nextState

                    if (newEnterHandler != NULL) newEnterHandler();
                }
            } else if (receivedEventBits != 0) { // Sensor event received
                 LOG_DEBUG(MODULE_MAIN, "Sensor event received: %x", receivedEventBits);
                 if (currentHandler != NULL) {
                     currentHandler(receivedEventBits); // Pass bits to state handler
                 } else {
                    // Optionally clear bits even if no handler:
                    // xEventGroupClearBits(sensorResultEventGroup, receivedEventBits);
                 }
            } else {
                 // Timeout occurred, no command, no sensor event
                 // Can perform periodic actions within a state if needed,
                 // or simply loop back to wait. Call currentHandler(0)?
                 if (currentHandler != NULL) {
                     currentHandler(0); // Indicate no specific event bit
                 }
            }
             // Optional small delay if loop isn't blocking enough
             // vTaskDelay(pdMS_TO_TICKS(10));
        }
        ```
    *   `[ ]` **Implement `enter_xxx()` functions:**
        *   Move initial mode setup logic here.
        *   **Send Start Events:** `xEventGroupSetBits(sensorControlEventGroup, BIT_START_TOF);` or `BIT_START_MMWAVE`.
        *   **Send Light Requests:** Create `LightEffectRequest_t request;` populate it, then `xQueueSend(lightEffectQueue, &request, 0);`. Example: `request.effect = EFFECT_TYPE_MANUAL_ON; xQueueSend...`
    *   `[ ]` **Implement `handle_state_xxx()` functions:**
        *   Move ongoing logic for that state here.
        *   Check the `eventBits` argument: `if (eventBits & BIT_TOF_DETECTED) { ... } else if (eventBits & BIT_MMWAVE_DETECTED) { ... } else if (eventBits == 0) { /* periodic check */ }`.
        *   Implement actions based on sensor events (e.g., send success light effect request, potentially determine `nextState` and trigger transition logic similar to command handling).
    *   `[ ]` **Implement `exit_xxx()` functions:**
        *   Move mode cleanup logic here.
        *   **Send Stop Events:** `xEventGroupSetBits(sensorControlEventGroup, BIT_STOP_TOF);` or `BIT_STOP_MMWAVE`.
        *   **Send Turn Off Light Request:** `request.effect = EFFECT_TYPE_OFF; xQueueSend...`.
    *   `[ ]` **`determineNextState()` helper:** Implement this function to map commands/events to state transitions based on the `currentState`.

**Phase 4: Update Interfacing Modules & Cleanup**

*   **Step 6: Refactor BLE Command Handling (`BluetoothControl`)**
    *   `[ ]` **Find Callback:** Locate the `onWrite` or similar callback function in `BluetoothControl.cpp`.
    *   `[ ]` **Parse & Create Struct:** Inside the callback, parse the received BLE data (`std::string` or `uint8_t*`). Create and populate a `BleCommand_t` struct.
    *   `[ ]` **Send to Queue:** Send the struct: `BaseType_t xHigherPriorityTaskWoken = pdFALSE; xQueueSendFromISR(cmdQueue, &commandData, &xHigherPriorityTaskWoken);` if in an ISR, otherwise use `xQueueSend(cmdQueue, &commandData, 0);`. Log queue errors (`errQUEUE_FULL`).
    *   `[ ]` **Yield if needed:** `if( xHigherPriorityTaskWoken ) { portYIELD_FROM_ISR(); }` if `xQueueSendFromISR` was used.

*   **Step 7: Review Mutex Usage**
    *   `[ ]` **Search:** Find all uses of `xSensorMutex`, `xMMWaveMutex`, `xObjectDetectedMutex`.
    *   `[ ]` **Remove Signaling Mutexes:** Delete `xSemaphoreTake`/`Give` calls associated only with now-removed flags. Delete the mutex creation if no longer needed.
    *   `[ ]` **Evaluate Remaining Mutexes:** Justify any remaining mutexes. Are they protecting *data* actively written/read by different tasks *simultaneously*? If event/queue flow avoids simultaneous access, remove the mutex. Minimize critical sections.
    *   `[ ]` **`DataControl` Safety:** Review if `DataControl` members are accessed by multiple tasks (e.g., `ProcessingTask` writing config, `SystemMonitorTask` reading battery thresholds). If so, add a mutex to `DataControl` or make access methods thread-safe. Prefer task-local copies or passing data via messages over shared state where feasible.

*   **Step 8: Testing and Validation**
    *   `[ ]` **Compile & Link:** Ensure the project builds without errors after each major step.
    *   `[ ]` **Unit Testing (Ideal):** If possible, test FSM logic, queue interactions, etc., in isolation.
    *   `[ ]` **Logging:** Add *detailed* logs (use `LOG_DEBUG`) at key points: Queue send/receive, Event group set/wait/received bits, FSM state transitions (enter/exit/handle), Task start/stop events.
    *   `[ ]` **Connectivity:** Test basic BLE connection, command sending, and confirm commands reach `cmdQueue` and `ProcessingTask`.
    *   `[ ]` **Task Coordination:** Verify sensor tasks start/stop based on `ProcessingTask` sending events. Verify light task receives and acts on queue messages.
    *   `[ ]` **Sensor Flow:** Test sensor detection -> `sensorResultEventGroup` bit set -> `ProcessingTask` receiving bit -> `handle_state_xxx` processing it.
    *   `[ ]` **Mode Logic:** Test *every* game mode: entry conditions, sensor interactions within the mode, expected light/buzzer feedback, exit conditions, transitions to other modes.
    *   `[ ]` **Background Task:** Confirm `SystemMonitorTask` performs its checks (heartbeat logs, BLE timeout behavior, battery logs).
    *   `[ ]` **Stress Test:** Send commands rapidly, trigger sensors frequently, check for deadlocks or missed events. Monitor task stack usage (`uxTaskGetStackHighWaterMark`).

---

## 4. Expected Benefits

*   **Improved Maintainability:** Clearer separation of concerns, FSM simplifies mode logic.
*   **Enhanced Scalability:** Easier to add new modes, sensors, or features.
*   **Better Responsiveness:** Event-driven design eliminates polling and unnecessary wake-ups.
*   **Increased Efficiency:** CPU cycles are used more effectively; tasks sleep when idle.
*   **Reduced Complexity:** While introducing RTOS concepts, the individual task logic becomes simpler and more focused.
*   **Improved Robustness:** Less reliance on shared flags reduces potential race conditions.

---

*(Optional: Include the updated Mermaid diagram from SYSTEM_ARCHITECTURE.md here if desired)*