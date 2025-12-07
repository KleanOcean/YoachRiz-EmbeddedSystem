#ifndef OTA_H
#define OTA_H

#ifndef NIMBLEDEVICE_H
#define  NIMBLEDEVICE_H
#include "NimBLEDevice.h"
#endif

/*
    Based on chegewara example for IDF: https://github.com/chegewara/esp32-OTA-over-BLE
    Ported to Arduino ESP32 by Claes Hallberg
    Licence: MIT
    OTA Bluetooth example between ESP32 (using NimBLE Bluetooth stack) and iOS swift (CoreBluetooth framework)
    Tested withh NimBLE v 1.3.1, iOS 14, ESP32 core 1.06
    N.B standard "nimconfig.h" needs to be customised (see below). In this example we only use the ESP32
    as perhipheral, hence no need to activate scan or central mode. Stack usage performs better for file transfer
    if stack is increased to 8192 Bytes
*/

#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_task_wdt.h>

class otaCallback: public BLECharacteristicCallbacks {

    private:

        BLECharacteristic * pTxCharacteristic = NULL;
        esp_ota_handle_t otaHandler = 0;
        uint8_t txValue = 0;
        int bufferCount = 0;
        const esp_partition_t *update_partition = NULL;
        bool downloadFlag = false;

    public:

        otaCallback( BLECharacteristic * pTxCharacteristic);
        void onWrite(BLECharacteristic * pCharacteristic);
        bool getDownloadFlag();
        void setDownloadFlag(bool val);

    
};
#endif