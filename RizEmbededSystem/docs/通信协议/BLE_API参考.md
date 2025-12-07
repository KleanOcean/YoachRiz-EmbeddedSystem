# Yoach 1 - BLE API 參考文件 (繁體中文)

本文件詳細說明了用於與 Yoach 1 設備互動的低功耗藍牙 (BLE) 通訊協定。

**目標讀者：** 行動應用程式開發人員 (iOS/Android)、測試人員、韌體開發人員。

---

## 目錄

- [Yoach 1 - BLE API 參考文件 (繁體中文)](#yoach-1---ble-api-參考文件-繁體中文)
  - [目錄](#目錄)
  - [1. BLE 服務 (Service) 與特徵 (Characteristics)](#1-ble-服務-service-與特徵-characteristics)
  - [2. 設備名稱](#2-設備名稱)
  - [3. 訊息格式：指令 (客戶端 -\> 設備)](#3-訊息格式指令-客戶端---設備)
    - [3.1 標準模式 (1-4 等)](#31-標準模式-1-4-等)
    - [3.2 節奏模式 (5)](#32-節奏模式-5)
    - [3.3 設定模式 (100)](#33-設定模式-100)
    - [3.4 其他模式 (Opening, Closing, Terminate)](#34-其他模式-opening-closing-terminate)
  - [4. 訊息格式：通知 (設備 -\> 客戶端)](#4-訊息格式通知-設備---客戶端)
  - [5. 空中韌體更新 (OTA - Over-The-Air)](#5-空中韌體更新-ota---over-the-air)
  - [6. 錯誤處理](#6-錯誤處理)

## 1. BLE 服務 (Service) 與特徵 (Characteristics)

*(注意：請將 `SERVICE_UUID` 等佔位符替換為 `Global_VAR.h` 中定義的實際值)*

*   **主要服務 UUID (Primary Service UUID):** `SERVICE_UUID`
    *   目的：包含 Yoach 1 主要通訊所需的所有特徵。
*   **特徵 (Characteristics):**
    *   **主要指令/通知 (`CHARACTERISTIC_MSG_UUID`)**
        *   **屬性 (Properties):** `WRITE`, `NOTIFY`
        *   **WRITE:** 客戶端（例如手機 App）將 CSV 指令字串寫入此特徵以控制設備。
        *   **NOTIFY:** 設備透過此特徵向客戶端發送通知（狀態更新、感測器觸發等）。客戶端必須啟用 (Subscribe) 此特徵的通知功能。
    *   **OTA 更新 (`CHARACTERISTIC_OTA_UUID`)**
        *   **屬性 (Properties):** `WRITE_NR` (Write No Response - 無回應寫入)
        *   **目的：** 專用於空中韌體更新 (OTA) 過程。新韌體的數據區塊會寫入此處。
    *   **TX 特徵 (`CHARACTERISTIC_TX_UUID`)**
        *   **屬性 (Properties):** `NOTIFY`
        *   **目的：** 可能用於輔助通知或除錯？*(需從程式碼/設計中釐清其確切作用)*。通常與 OTA 配合使用。

## 2. 設備名稱

*   設備進行藍牙廣播時的名稱通常格式化為 `Yoach1-XXXX`，其中 `XXXX` 源自 ESP32 的 MAC 地址。

## 3. 訊息格式：指令 (客戶端 -> 設備)

所有指令都是以 **逗號分隔值 (CSV - Comma-Separated Value) 字串** 的形式，寫入 `CHARACTERISTIC_MSG_UUID` 特徵。設備會根據第一個值（遊戲模式 ID）來解析字串。所有指令預期包含 **8 個欄位**，對於某些模式下未使用的參數，請使用佔位符（例如 `999`）。

### 3.1 標準模式 (1-4 等)

*   **格式:** `gameMode,blinkBreak,timedBreak,buzzer,buzzerTime,buffer,doubleModeIndex,process`
*   **欄位說明:**
    *   `gameMode` (int): 目標模式 ID (例如, 1=MANUAL, 2=RANDOM, 3=TIMED, 4=DOUBLE)。
    *   `blinkBreak` (int, ms): 閃爍間隔時間 (如果適用於該模式)。*常未使用/佔位符。*
    *   `timedBreak` (int, ms): `TIMED_MODE` 的超時時間。*其他模式常未使用/佔位符。*
    *   `buzzer` (int, 0 或 1): 啟用/禁用蜂鳴器回饋 (1=ON, 0=OFF)。*常未使用/佔位符。*
    *   `buzzerTime` (int, ms): 蜂鳴器鳴響持續時間。*常未使用/佔位符。*
    *   `buffer` (int): 感測器靈敏度緩衝區 (確切含義待定，內部可能常+1)。*常未使用/佔位符。*
    *   `doubleModeIndex` (int): `DOUBLE_MODE` 的索引值 (0-N)。*其他模式常未使用/佔位符。*
    *   `process` (int): 內部處理標誌或數值 (含義可能變化)。*常未使用/佔位符。*
*   **範例 (設定 Timed Mode，超時 5000ms):** `3,999,5000,999,999,999,999,999`
*   **範例 (設定 Manual Mode):** `1,999,999,999,999,999,999,999`

### 3.2 節奏模式 (5)

*   **格式:** `5,red,green,blue,timerValue,buzzerValue,sensorMode,placeholder`
*   **欄位說明:**
    *   `gameMode` (int): 5
    *   `red` (int, 0-255): 紅色值。
    *   `green` (int, 0-255): 綠色值。
    *   `blue` (int, 0-255): 藍色值。
    *   `timerValue` (int, ms): 自動關燈計時器持續時間。0 表示燈光保持亮著，直到感測器偵測到物體 (如果 `sensorMode` > 0) 或無限期亮著 (如果 `sensorMode` == 0)。
    *   `buzzerValue` (int, ms): 蜂鳴器持續時間。0 表示在此指令下禁用蜂鳴器。
    *   `sensorMode` (int, 0-3): 感測器配置:
        *   `0`: 無感測器偵測。燈光僅由 `timerValue` 控制。
        *   `1`: 僅啟用 LiDAR (TOF) 感測器。
        *   `2`: 僅啟用 MMWave 雷達感測器。
        *   `3`: 同時啟用 LiDAR 和 MMWave 感測器。
    *   `placeholder` (int): **必須**是 `999`。保留欄位。
*   **行為:** 設定 LED 顏色。如果 `timerValue > 0`，燈光在該時間後熄滅。如果 `timerValue == 0` 且 `sensorMode > 0`，燈光在感測器偵測到物體後熄滅。如果 `timerValue == 0` 且 `sensorMode == 0`，燈光保持亮著。如果 `buzzerValue > 0`，蜂鳴器響 `buzzerValue` 毫秒。根據 `sensorMode` 啟用感測器。
*   **範例 (橘燈, 3秒計時器, 無蜂鳴器, 啟用雙感測器):** `5,255,165,0,3000,0,3,999`
*   **範例 (藍燈, 持續亮著直到 LiDAR 偵測, 1秒蜂鳴器):** `5,0,0,255,0,1000,1,999`
*   **模式內更新:** 如果設備*已經*處於節奏模式時，收到另一個節奏模式指令，則顏色、計時器、蜂鳴器和感測器配置會被更新，而*不會*完全重啟模式或感測器偵測（除非 `sensorMode` 發生變化）。

### 3.3 設定模式 (100)

*   **格式:** `100,blinkCount,999,999,999,999,999,999`
*   **欄位說明:**
    *   `gameMode` (int): 100
    *   `blinkCount` (int): LED 應閃爍以提供設定回饋的次數。
    *   *其他欄位為佔位符 (`999`)。*
*   **行為:** 使用 `blinkCount` 觸發 `configNumberWipe` 燈光/蜂鳴器序列。設備通常在此之後轉換到 `PROCESSED_MODE`。
*   **範例 (閃爍 4 次):** `100,4,999,999,999,999,999,999`

### 3.4 其他模式 (Opening, Closing, Terminate)

*   這些模式通常使用標準的 8 欄位格式，但主要依賴 `gameMode` ID。其他欄位使用佔位符 (`999`)。
*   **範例 (Terminate 模式):** `13,999,999,999,999,999,999,999`
*   **範例 (Opening 模式):** `11,999,999,999,999,999,999,999`

## 4. 訊息格式：通知 (設備 -> 客戶端)

設備透過 `CHARACTERISTIC_MSG_UUID` 特徵發送簡單的 **字串** 通知。客戶端必須啟用此特徵的通知。

*   **`manual`:** 當 TOF 感測器在 `MANUAL_MODE` 下偵測到物體時發送。
*   **`random`:** 當 TOF 感測器在 `RANDOM_MODE` 下偵測到物體時發送。
*   **`timed`:** 當 MMWave 感測器在 `TIMED_MODE` 下偵測到物體時發送。
*   **`rhythm`:** 當啟用的感測器 (TOF 或 MMWave，基於 `sensorMode`) 在 `RHYTHM_MODE` 下偵測到物體時發送。
*   **`double<index>`:** 當在 `DOUBLE_MODE` 下發生偵測時發送，其中 `<index>` 是當前的 `doubleModeIndex` (例如 `double0`, `double1`)。
*   **`Timed Mode Overtimed`:** 如果在 `TIMED_MODE` 下 `timedBreak` 時間耗盡而未偵測到物體時發送。
*   **`configDone:<count>`:** 在 `CONFIG_MODE` 下 `configNumberWipe` 序列完成後發送，其中 `<count>` 是使用的 `blinkCount` (例如 `configDone:4`)。
*   **(其他狀態/錯誤訊息):** 可能會定義其他用於電池狀態、錯誤等的通知。*(請檢查韌體實現以獲取其他訊息)*。

## 5. 空中韌體更新 (OTA - Over-The-Air)

*   OTA 更新透過 `CHARACTERISTIC_OTA_UUID` 處理。
*   客戶端應用程式遵循特定協定（通常涉及按順序寫入韌體區塊）來上傳新的韌體二進制文件。
*   韌體中的 `otaCallback` 類別負責處理這些寫入操作。
*   進度或狀態可能透過 `CHARACTERISTIC_TX_UUID` 或 `CHARACTERISTIC_MSG_UUID` 回傳。*(詳情請參考 `otaCallback.cpp` 的實現)*。

## 6. 錯誤處理

*   **無效指令格式:** 如果設備收到無法正確解析的指令字串（例如，欄位數量錯誤），它會在內部記錄錯誤 (`[ERROR][DATA] Invalid message format...`)，並且通常會忽略該指令，保持當前狀態。通常不會透過 BLE 向客戶端發送特定的錯誤訊息。
*   **無效參數:** 韌體可能具有內部參數範圍檢查（例如，RGB 值 0-255）。對於無效參數的行為取決於實現（可能會限制數值或忽略指令）。

---