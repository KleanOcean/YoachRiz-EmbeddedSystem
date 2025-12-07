# OTA Protocol Implementation Guide
## Based on iOS Proven Implementation

**Version:** 2.0.0
**Last Updated:** 2025-11-23
**Status:** Production-Ready Protocol

---

## 🎯 Executive Summary

This document defines the OTA protocol implementation based on the **successfully proven iOS implementation**. The iOS app has been reliably updating Riz (formerly Yoach1) devices in production, and this protocol should be exactly replicated in our Python BLE GUI.

---

## 📱 iOS Protocol Analysis (PROVEN WORKING)

### Key Protocol Parameters from iOS

| Parameter | iOS Value | Why It Works |
|-----------|-----------|--------------|
| **MTU** | 517 bytes max | ESP32 NimBLE negotiated value |
| **Chunk Size** | `MTU - 3` (~514 bytes) | Accounts for ATT header overhead |
| **Write Type** | `WRITE_NO_RESPONSE` | Fast, no per-packet ACK |
| **Chunks per ACK** | 2 chunks | Burst mode with periodic ACK |
| **ACK Wait** | After every 2 chunks | Device sends 1-byte confirmation |
| **Inter-device delay** | 10 seconds | For multi-device updates |

### iOS Transfer Flow (MUST REPLICATE)

```
1. Calculate chunk size = peripheral.maximumWriteValueLength(.withoutResponse) - 3
2. Copy firmware to dataBuffer
3. While dataBuffer not empty:
   a. Extract chunk from buffer
   b. Write to OTA characteristic (WRITE_NO_RESPONSE)
   c. Increment chunkCount
   d. If chunkCount == 2:
      - Wait for 1-byte ACK from device
      - Reset chunkCount to 0
   e. Update progress
4. Transfer complete when buffer empty
```

### Critical iOS Code Reference

```swift
// DM_OTA.swift:60-115
func writeDataToPeripheral() {
    // Key variables
    ota.chunkCount = 2  // CRITICAL: 2 chunks per ACK cycle

    while !ota.dataBuffer.isEmpty && peripheral.canSendWriteWithoutResponse {
        // Extract chunk
        let chunk = dataBuffer.prefix(chunkSize)

        // Send without response
        peripheral.writeValue(chunk, for: otaCharacteristic, type: .withoutResponse)

        // Update counters
        ota.packageCounter += 1

        // Handle ACK cycle
        if ota.packageCounter % ota.chunkCount == 0 {
            // Device will send 1-byte ACK
            // iOS continues after receiving notification
        }
    }
}
```

---

## 🔄 Python Implementation Requirements

### MUST MATCH iOS EXACTLY

#### 1. Chunk Size Calculation
```python
# iOS: MTU - 3 bytes for ATT header
# Default: 517 - 3 = 514 bytes
# But iOS may negotiate different MTU
CHUNK_SIZE = 514  # Use large chunks like iOS
```

#### 2. Write Without Response
```python
# iOS uses WRITE_NO_RESPONSE for speed
await client.write_gatt_char(
    OTA_CHAR_UUID,
    chunk_data,
    response=False  # CRITICAL: Must be False
)
```

#### 3. Two-Chunk Burst Pattern
```python
# iOS sends 2 chunks, then waits for ACK
chunks_sent = 0
for chunk in firmware_chunks:
    # Send chunk
    await send_chunk(chunk)
    chunks_sent += 1

    # Every 2 chunks, wait for ACK
    if chunks_sent % 2 == 0:
        await wait_for_ack()  # 1-byte notification
```

#### 4. No Additional Headers
```python
# iOS sends RAW firmware data
# NO offset headers
# NO control bytes
# Just pure firmware binary chunks
```

---

## ❌ Current Python Implementation Issues

### What We're Doing Wrong

| Issue | Current (Wrong) | iOS (Correct) |
|-------|----------------|---------------|
| **Chunk Size** | 16-20 bytes | 514 bytes |
| **ACK Pattern** | Every chunk | Every 2 chunks |
| **Data Format** | Adding headers | Raw firmware only |
| **Initial Packet** | Control bytes | Direct firmware data |
| **Timing** | Too much delay | Minimal delay |

### Why Current Implementation Fails

1. **Too Small Chunks**: 16-byte chunks vs iOS's 514-byte chunks = 32x more overhead
2. **Too Many ACKs**: Waiting after every chunk instead of every 2 chunks
3. **Wrong Data Format**: ESP32 expects raw firmware, not wrapped data
4. **Protocol Mismatch**: ESP32 firmware is tuned for iOS protocol

---

## ✅ Corrected Python Implementation

### Updated Configuration

```python
class OTAUploader:
    # Match iOS exactly
    CHUNK_SIZE = 514      # iOS uses MTU-3 (typically 514)
    CHUNKS_PER_ACK = 2    # iOS sends 2 chunks per ACK
    ACK_TIMEOUT = 2       # Reasonable timeout
    MAX_RETRIES = 3       # Retry on failure
```

### Updated Transfer Logic

```python
async def transfer_firmware(self, firmware_data):
    """
    Transfer firmware using iOS protocol exactly
    """
    total_size = len(firmware_data)
    offset = 0
    chunk_counter = 0

    while offset < total_size:
        # Calculate chunk size (may be smaller for last chunk)
        chunk_size = min(self.CHUNK_SIZE, total_size - offset)
        chunk = firmware_data[offset:offset + chunk_size]

        # Send chunk WITHOUT any headers (iOS style)
        await self.client.write_gatt_char(
            OTA_CHAR_UUID,
            chunk,
            response=False  # WRITE_NO_RESPONSE
        )

        offset += chunk_size
        chunk_counter += 1

        # Every 2 chunks, wait for ACK (iOS style)
        if chunk_counter % self.CHUNKS_PER_ACK == 0:
            # Wait for 1-byte ACK notification
            ack_received = await self.wait_for_ack()
            if not ack_received:
                # Retry logic
                return False

        # Update progress
        progress = (offset / total_size) * 100
        self.update_progress(progress)

    # Final ACK if odd number of chunks
    if chunk_counter % self.CHUNKS_PER_ACK != 0:
        await self.wait_for_ack()

    return True
```

### Notification Handler for ACK

```python
def notification_handler(self, sender, data):
    """
    Handle notifications from ESP32
    iOS expects 1-byte ACK after every 2 chunks
    """
    if len(data) == 1:
        # This is the ACK iOS waits for
        self.ack_received = True
        print(f"ACK received: 0x{data[0]:02x}")
```

---

## 📊 Performance Comparison

### Transfer Speed Expectations

| Implementation | Chunk Size | ACKs | Expected Speed | 586KB Time |
|----------------|-----------|------|----------------|------------|
| **iOS (Working)** | 514 bytes | Every 1KB | 15-25 KB/s | 25-40 sec |
| **Python (Old)** | 16 bytes | Every 16B | <1 KB/s | >10 min |
| **Python (Fixed)** | 514 bytes | Every 1KB | 15-25 KB/s | 25-40 sec |

---

## 🔧 Implementation Checklist

### Phase 1: Protocol Alignment
- [ ] Change `CHUNK_SIZE` from 16 to 514 bytes
- [ ] Change `CHUNKS_PER_ACK` from 1 to 2
- [ ] Remove all header additions (offset bytes, control bytes)
- [ ] Ensure `response=False` on all writes
- [ ] Implement 2-chunk burst pattern

### Phase 2: ACK Handling
- [ ] Subscribe to TX characteristic for notifications
- [ ] Wait for 1-byte ACK after every 2 chunks
- [ ] Handle ACK timeout gracefully
- [ ] Add retry mechanism on ACK failure

### Phase 3: Testing
- [ ] Verify first chunk contains 0xE9 magic byte
- [ ] Test with MINI device (586 KB firmware)
- [ ] Test with PRO device (595 KB firmware)
- [ ] Verify ~25-40 second transfer time
- [ ] Test multi-device sequential updates

---

## 🚀 Migration Path

### Step 1: Backup Current Code
```bash
cp ota_uploader.py ota_uploader_old.py
cp ble_manager.py ble_manager_old.py
```

### Step 2: Update Constants
```python
# Old (wrong)
CHUNK_SIZE = 16
CHUNKS_PER_ACK = 1

# New (iOS-matching)
CHUNK_SIZE = 514
CHUNKS_PER_ACK = 2
```

### Step 3: Update Send Logic
```python
# Old (wrong)
packet = struct.pack('<I', offset) + data  # NO!

# New (iOS-matching)
packet = data  # Just raw data, no headers
```

### Step 4: Update ACK Pattern
```python
# Old (wrong)
for chunk in chunks:
    send(chunk)
    wait_ack()  # Every chunk

# New (iOS-matching)
for i, chunk in enumerate(chunks):
    send(chunk)
    if (i + 1) % 2 == 0:  # Every 2 chunks
        wait_ack()
```

---

## 📝 ESP32 Firmware Expectations

Based on iOS working implementation, the ESP32 firmware expects:

1. **Raw firmware data** starting with magic byte 0xE9
2. **Large chunks** (514 bytes typical)
3. **WRITE_NO_RESPONSE** for speed
4. **ACK after 2 chunks** (burst mode)
5. **No offset headers** in data packets

The ESP32's OTA handler (`BluetoothControl.cpp:109-110`):
```cpp
pOtaCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_OTA_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
);
```

This supports both WRITE and WRITE_NO_RESPONSE, but iOS uses WRITE_NO_RESPONSE for performance.

---

## 🎯 Success Criteria

The Python implementation will be considered successful when:

1. ✅ Firmware transfers complete in 25-40 seconds (matching iOS)
2. ✅ No "invalid magic byte" errors
3. ✅ No connection drops during transfer
4. ✅ Device successfully reboots with new firmware
5. ✅ Multi-device updates work with 10-second delays

---

## 📚 References

- **iOS Implementation**: `/ios/V0_stable/YOICHI/ViewModel/DM_OTA.swift`
- **ESP32 OTA Handler**: `/EmbededSystem/src/BluetoothControl.cpp`
- **BLE Specification**: Bluetooth Core Spec v5.0, Vol 3, Part F (ATT)

---

## 🔴 Critical Points

### DO NOT:
- ❌ Add offset headers to packets
- ❌ Send control bytes before firmware
- ❌ Use small chunk sizes (<500 bytes)
- ❌ Wait for ACK after every chunk
- ❌ Use WRITE_WITH_RESPONSE

### ALWAYS:
- ✅ Send raw firmware data only
- ✅ Use 514-byte chunks (MTU-3)
- ✅ Send 2 chunks per ACK cycle
- ✅ Use WRITE_NO_RESPONSE
- ✅ Start with 0xE9 magic byte

---

**End of Protocol Documentation**