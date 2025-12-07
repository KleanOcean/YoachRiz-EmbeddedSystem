# OTA (Over-The-Air) Update Implementation

**Version:** 1.0.0
**Last Updated:** 2025-11-23
**Platform:** iOS (V0_stable)
**Firmware Compatibility:** Yoach 1 v0.1.0

---

## Overview

The YOICHI iOS app includes a complete Over-The-Air (OTA) firmware update system that allows users to wirelessly update the firmware on connected Yoach 1 devices (MINI and PRO models).

---

## Table of Contents

- [Architecture](#architecture)
- [File Locations](#file-locations)
- [BLE Characteristics](#ble-characteristics)
- [Firmware Sources](#firmware-sources)
- [Update Process](#update-process)
- [Code Structure](#code-structure)
- [Usage](#usage)
- [Technical Details](#technical-details)

---

## Architecture

### Component Overview

```
┌─────────────────┐
│   OTAView.swift │ ◄── User Interface
│  (SwiftUI View) │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  DM_OTA.swift   │ ◄── Business Logic
│ (DataModel Ext) │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   OTA.swift     │ ◄── Data Model & BLE Config
│ (Model + UUID)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  firmware/*.bin │ ◄── Firmware Binaries
└─────────────────┘
```

---

## File Locations

### iOS App Files

| File | Path | Purpose |
|------|------|---------|
| **OTA.swift** | `/YOICHI/Model/OTA.swift` | Data structures, BLE UUIDs, OTA state management |
| **DM_OTA.swift** | `/YOICHI/ViewModel/DM_OTA.swift` | OTA update logic, firmware transfer, version management |
| **OTAView.swift** | `/YOICHI/View/OTAView.swift` | User interface for OTA updates |

### Firmware Files

| File | Path | Size | Device Type |
|------|------|------|-------------|
| **firmware-mini.bin** | `/firmware/firmware-mini.bin` | 586 KB | MINI devices |
| **firmware-pro.bin** | `/firmware/firmware-pro.bin` | 595 KB | PRO devices |
| **firmware.bin** | `/firmware/firmware.bin` | 586 KB | Testing/Generic |

**Note:** Firmware files are bundled with the iOS app and can also be fetched from GitHub.

---

## BLE Characteristics

### Service UUID
```
ab0828b1-198e-4351-b779-901fa0e0371e
```

### OTA-Specific Characteristics

| Characteristic | UUID | Type | Purpose |
|----------------|------|------|---------|
| **OTA Data** | `62EC0272-3EC5-11EB-B378-0242AC130005` | WRITE_NO_RESPONSE | Firmware data chunks |
| **Status/TX** | `62EC0272-3EC5-11EB-B378-0242AC130003` | NOTIFY | Transfer acknowledgments |
| **Message** | `4ac8a696-9736-4e5d-932b-e9b31405049c` | READ/WRITE/NOTIFY | General communication |

**File Reference:** `OTA.swift:14-20`

---

## Firmware Sources

### 1. Local (Bundled)

Firmware binaries are **embedded in the iOS app bundle** during compilation.

**Location:** `/firmware/*.bin`

**Usage:** "Stable" firmware version option

**Advantages:**
- Always available offline
- Fast access
- No network required
- Guaranteed compatibility

**Code Reference:** `DM_OTA.swift:117-131`

```swift
func getFMFromLocalFile(fileName:String)throws -> Data? {
    guard let fileURL = Bundle.main.url(forResource: fileName, withExtension: ".bin")
    else { return nil }

    do {
        let fileData = try Data(contentsOf: fileURL)
        return Data(fileData)
    }
    catch {
        print("Error loading file: \(error)")
        return nil
    }
}
```

---

### 2. Remote (GitHub)

Latest firmware can be fetched from **GitHub repository**.

**GitHub URLs:**

- **MINI:** `https://github.com/KleanOcean/Fireware_base/blob/master/mini/firmware.bin?raw=true`
- **PRO:** `https://github.com/KleanOcean/Fireware_base/blob/master/pro/firmware.bin?raw=true`
- **Generic:** `https://github.com/KleanOcean/Fireware_base/blob/master/firmware.bin?raw=true`

**Usage:** "Latest" firmware version option

**Advantages:**
- Always up-to-date
- No app recompilation needed
- Centralized firmware distribution

**Disadvantages:**
- Requires internet connection
- Download time varies
- Potential network failures

**Code Reference:** `DM_OTA.swift:133-163`

```swift
func getMiniFMFromWeb()throws -> Data? {
    do {
        let fileData = try Data(contentsOf: URL(string:
            "https://github.com/KleanOcean/Fireware_base/blob/master/mini/firmware.bin?raw=true")!)

        ota.FM_status = "latest Firmwares updated"
        return Data(fileData)
    }
    catch {
        print("Error loading file: \(error)")
        return nil
    }
}
```

---

## Update Process

### Firmware Version Selection

The app supports **3 firmware version modes**:

| Index | Mode | Source | Use Case |
|-------|------|--------|----------|
| **0** | Stable | Local bundle | Production, reliable updates |
| **1** | Latest | GitHub remote | Get newest features |
| **2** | Testing | Local bundle (firmware.bin) | Development, debugging |

**Code Reference:** `DM_OTA.swift:177-202`

---

### Update Flow

#### Single Device Update

```
User selects device → Choose version → Tap "Update D1"
         ↓
chooseSuitableFMFor(index, version, mini_FM, pro_FM)
         ↓
Determine device model (MINI/PRO)
         ↓
Load appropriate firmware binary
         ↓
sendFile(firmwareData, deviceIndex)
         ↓
Transfer firmware via BLE chunks
         ↓
Device receives, validates, flashes firmware
         ↓
Update complete / Device reboots
```

**Code Reference:** `DM_OTA.swift:231-237`

```swift
func updateIndivdualDevice(_ index : Int, _ FMVersionIndex:Int,
                          _ miniAppStorage:Data, _ proAppStorage:Data) {

    ota.tempFirmware = chooseSuitableFMFor(index, FMVersionIndex,
                                          miniAppStorage, proAppStorage)

    sendFile(FM: ota.tempFirmware!, phIndex: ota.nextDeviceToUpdate)
}
```

---

#### Multi-Device Update (Update All)

```
User taps "Update All" → Choose version
         ↓
updateAllDevices(version, mini_FM, pro_FM)
         ↓
Set updateAll flag = true
         ↓
For each connected device:
    ├─ Send firmware to device
    ├─ Wait 10 seconds
    └─ Move to next device
         ↓
All devices updated
```

**Code Reference:** `DM_OTA.swift:239-249`

```swift
func updateAllDevices(_ FMVersionIndex:Int, _ miniAppStorage:Data,
                     _ proAppStorage:Data) {

    ota.tempFirmware = chooseSuitableFMForAll(FMVersionIndex,
                                             miniAppStorage, proAppStorage)

    ota.updateAll = true
    ota.totalDevicesToUpdate = howManyIsHere() - 1
    ota.nextDeviceToUpdate = 0

    sendFile(FM: ota.tempFirmware!, phIndex: ota.nextDeviceToUpdate)
}
```

**Sequential Update Logic:** `DM_OTA.swift:87-101`

After each device completes:
- **10-second delay** between devices
- Automatically moves to next device
- Updates status when all complete

---

### Firmware Transfer Protocol

#### Chunked Transfer

Firmware is sent in **small chunks** to accommodate BLE MTU limitations.

**Chunk Size Calculation:**
```swift
ota.chunkSize = peripheral.maximumWriteValueLength(for: .withoutResponse) - 3
```

Typical chunk size: **~512 bytes** (depends on negotiated MTU)

**Code Reference:** `DM_OTA.swift:60-115`

#### Transfer Process

1. **Initialize Transfer**
   - Copy firmware data to `ota.dataBuffer`
   - Set `ota.dataLength` = total bytes
   - Set `ota.transferOngoing = true`
   - Reset counters and timers

2. **Send Chunks**
   - While `dataBuffer` not empty AND peripheral ready:
     - Extract chunk from buffer
     - Write to OTA characteristic (WRITE_NO_RESPONSE)
     - Remove sent data from buffer
     - Update progress percentage
     - Calculate transfer speed (KB/s)

3. **Acknowledgment**
   - Device sends 1-byte acknowledgment
   - iOS continues sending next batch
   - `chunkCount = 2` chunks per batch cycle

4. **Completion**
   - When buffer empty, set `transferOngoing = false`
   - Device validates and flashes firmware
   - Device reboots automatically

**Transfer Speed Tracking:**
```swift
ota.elapsedTime = CFAbsoluteTimeGetCurrent() - ota.startTime
let kbPs = Double(ota.sentBytes) / ota.elapsedTime
ota.kBPerSecond = kbPs / 1000
```

---

## Code Structure

### OTA.swift (Model)

**Purpose:** Data structures and BLE configuration

**Key Components:**

1. **BLE UUIDs**
   ```swift
   let serviceUUID = CBUUID(string:"ab0828b1-198e-4351-b779-901fa0e0371e")
   let otaCharacteristicId = CBUUID(string: "62EC0272-3EC5-11EB-B378-0242AC130005")
   let statusCharacteristicId = CBUUID(string: "62EC0272-3EC5-11EB-B378-0242AC130003")
   ```

2. **OTA State Structure**
   ```swift
   struct OTA {
       var transferProgress : Double = 0.0
       var chunkCount = 2
       var elapsedTime = 0.0
       var kBPerSecond = 0.0
       var updateAll = false
       var totalDevicesToUpdate = 0
       var nextDeviceToUpdate = -1
       var tempFirmware : Data?
       var FM_status = "local firmware found"

       // Transfer variables
       var dataToSend = Data()
       var dataBuffer = Data()
       var chunkSize = 0
       var dataLength = 0
       var transferOngoing = false
       var sentBytes = 0
       var packageCounter = 0
       var startTime = 0.0
       var stopTime = 0.0
       var firstAcknowledgeFromESP32 = false
   }
   ```

---

### DM_OTA.swift (Business Logic)

**Purpose:** Firmware transfer and version management logic

**Key Functions:**

| Function | Line | Purpose |
|----------|------|---------|
| `sendFile()` | 13-33 | Transfer firmware to specific device |
| `sendFile2()` | 36-52 | Transfer pre-loaded firmware |
| `writeDataToPeriheral()` | 60-115 | BLE chunk transfer loop |
| `getFMFromLocalFile()` | 117-131 | Load bundled firmware |
| `getMiniFMFromWeb()` | 133-148 | Fetch MINI firmware from GitHub |
| `getProFMFromWeb()` | 150-163 | Fetch PRO firmware from GitHub |
| `chooseSuitableFMFor()` | 177-202 | Select firmware by version & model |
| `updateIndivdualDevice()` | 231-237 | Update single device |
| `updateAllDevices()` | 239-249 | Update all connected devices |

---

### OTAView.swift (User Interface)

**Purpose:** SwiftUI view for OTA operations

**Key UI Elements:**

1. **Firmware Version Picker**
   - Segmented control: Stable / Latest / Testing
   - Stored in `@State var FMVersionIndex`

2. **Check Updates Button**
   - Fetches latest firmware from GitHub
   - Stores in `@AppStorage` for persistence

3. **Individual Update Buttons**
   - One button per connected device
   - Format: "Update D1", "Update D2", etc.

4. **Update All Button**
   - Sequential update of all devices
   - 10-second delay between devices

5. **Progress Display**
   - Transfer percentage
   - Transfer speed (KB/s)
   - Status messages

**Code Reference:** `OTAView.swift:17-100`

---

## Usage

### User Workflow

#### Step 1: Check for Updates (Optional)

1. Open OTA view
2. Tap **"Check Updates"** button
3. App downloads latest firmware from GitHub
4. Firmware cached in `@AppStorage`

#### Step 2: Select Firmware Version

- **Stable:** Bundled firmware (reliable)
- **Latest:** Downloaded from GitHub (newest)
- **Testing:** Generic firmware.bin (development)

#### Step 3: Update Device(s)

**Single Device:**
1. Tap **"Update D1"** (or D2, D3, etc.)
2. Watch progress indicator
3. Wait for completion (~20-30 seconds)
4. Device reboots automatically

**All Devices:**
1. Tap **"Update All"**
2. Devices updated sequentially
3. 10-second delay between devices
4. All devices reboot when done

---

## Technical Details

### BLE Transfer Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| **MTU** | 517 bytes max | ESP32 BLE stack limit |
| **Chunk Size** | MTU - 3 bytes | Account for BLE headers |
| **Write Type** | WRITE_NO_RESPONSE | Faster, no per-chunk ACK |
| **Chunks per Cycle** | 2 | Controlled by `chunkCount` |
| **Device ACK** | 1 byte | Signals ready for next batch |

### Transfer Speed

**Typical Performance:**
- Transfer speed: **~15-25 KB/s**
- 586 KB firmware: **~25-40 seconds**
- 595 KB firmware: **~25-40 seconds**

**Factors Affecting Speed:**
- BLE connection quality
- iOS device processing
- ESP32 flash write speed
- MTU negotiation

### Device Model Detection

**Code Reference:** `DM_Game_Helper.swift:99-109`

```swift
func tellMeDeviceModel(index: Int) -> String {
    if peripherals[index].name.hasPrefix("MINI") {
        return "mini"
    }
    if peripherals[index].name.hasPrefix("PRO") {
        return "pro"
    }
    if peripherals[index].name.hasPrefix("S3PRO") {
        return "S3pro"
    }
    return "unknown"
}
```

Device name pattern determines which firmware to use.

### Firmware Comparison

**Code Reference:** `DM_OTA.swift:165-174`

The app can compare local vs. remote firmware by checking the **last 10 bytes** of each binary:

```swift
func compareFMsEquals(_ FM_AP:Data, _ FM_Web:Data) -> Bool {
    let AP_Bytes = FM_AP[FM_AP.count-10..<FM_AP.count-1]
    let Web_Bytes = FM_Web[FM_Web.count-10..<FM_Web.count-1]
    let AP_val = AP_Bytes.hexEncodedString(options: .upperCase)
    let Web_val = Web_Bytes.hexEncodedString(options: .upperCase)

    return AP_val == Web_val
}
```

**Use Case:** Determine if downloaded firmware differs from bundled version.

---

## State Management

### OTA Transfer State

```swift
// Active transfer
ota.transferOngoing = true  // UI buttons disabled
ota.transferProgress = 45.3 // Progress percentage
ota.kBPerSecond = 18.5     // Transfer speed

// Multi-device update
ota.updateAll = true
ota.nextDeviceToUpdate = 1  // Currently updating D2
ota.totalDevicesToUpdate = 3 // 4 devices total (0-3)
```

### UI State Management

**Disabled During Transfer:**
- Navigation back button
- All update buttons
- Version picker

**Code Reference:** `OTAView.swift:53-66`

```swift
.disabled(dataModel.ota.transferOngoing)
.navigationBarBackButtonHidden(dataModel.ota.transferOngoing)
```

---

## Error Handling

### Common Error Scenarios

1. **File Not Found**
   - Local firmware missing from bundle
   - Returns `nil`, prints error message

2. **Network Failure**
   - GitHub download fails
   - Catches error, returns `nil`

3. **BLE Disconnection**
   - Transfer interrupted mid-update
   - Device may require manual recovery

4. **Invalid Firmware**
   - Device validates firmware signature
   - ESP32 automatically rolls back if invalid

### Recovery

**Device-Side:**
- ESP32 OTA partition validation
- Automatic rollback on boot failure
- Watchdog timer protection

**iOS-Side:**
- User must retry update
- Check connection status
- Verify firmware file exists

---

## Limitations

1. **Sequential Multi-Device Updates**
   - Cannot update devices in parallel
   - 10-second delay between devices
   - Total time = (devices × 30s) + (devices-1 × 10s)

2. **No Resume Support**
   - Disconnection requires full restart
   - No partial transfer recovery

3. **No Version Verification**
   - App doesn't query current device firmware version
   - User must track versions manually

4. **Fixed Chunk Count**
   - `chunkCount = 2` is hardcoded
   - Could be optimized for faster transfers

---

## Future Improvements

### Potential Enhancements

1. **Parallel Updates**
   - Update multiple devices simultaneously
   - Reduce total update time

2. **Resume Capability**
   - Save transfer state
   - Resume from last successful chunk

3. **Version Checking**
   - Query device firmware version
   - Only update if newer version available

4. **Compression**
   - Compress firmware before transfer
   - Decompress on device

5. **Delta Updates**
   - Only transfer changed portions
   - Significantly faster updates

6. **Progress Notifications**
   - Background update support
   - Push notifications on completion

---

## Debugging

### Enable Logging

Look for these debug prints during OTA:

```
FUNC SendFile
file transfer: 23.4%
Update done
Update All Done
Error loading file: <error>
```

### Monitor Transfer

```swift
print("Transfer progress: \(ota.transferProgress)%")
print("Speed: \(ota.kBPerSecond) KB/s")
print("Elapsed: \(ota.elapsedTime)s")
```

### Check Firmware Loaded

```swift
print("Firmware size: \(ota.tempFirmware?.count ?? 0) bytes")
print("Status: \(ota.FM_status)")
```

---

## Security Considerations

### Current Implementation

- **No encryption** on firmware transfer
- **No authentication** of firmware source
- **BLE security** depends on OS-level pairing

### Recommendations for Production

1. **Firmware Signing**
   - Digital signature verification
   - Prevent unauthorized firmware

2. **Encrypted Transfer**
   - Encrypt firmware during BLE transfer
   - Prevent man-in-the-middle attacks

3. **Secure Boot**
   - ESP32 secure boot feature
   - Only allow signed firmware

4. **HTTPS**
   - Already using HTTPS for GitHub downloads
   - Ensures firmware integrity from server

---

## Related Documentation

- **BLE Protocol:** See `GAME_MODES.md` Section 10 "Over-The-Air Updates"
- **Device Naming:** See `GAME_MODES.md` Section 3 "Device Naming"
- **Firmware Repository:** `https://github.com/KleanOcean/Fireware_base`

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-11-23 | Initial OTA implementation documentation |

---

**End of Documentation**
