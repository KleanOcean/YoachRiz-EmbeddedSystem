#include "BluetoothControl.h"
#include "Log.h"

/*------------------------------------------------------------------------------
  BLE Server callback
  ----------------------------------------------------------------------------*/
void MyServerCallbacks::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {

    /*----------------------------------------
      * BLE Power settings. P9 = max power +9db
      ---------------------------------------*/
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL1, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

    const char* deviceAddress = NimBLEAddress(desc->peer_ota_addr).toString().c_str();
    LOG_INFO(MODULE_BLE, "Device connected: %s", deviceAddress);
    
    /*    We can use the connection handle here to ask for different connection parameters.
          Args: connection handle, min connection interval, max connection interval
          latency, supervision timeout.
          Units; Min/Max Intervals: 1.25 millisecond increments.
          Latency: number of intervals allowed to skip.
          Timeout: 10 millisecond increments, try for 5x interval time for best results.
    */
    pServer->updateConnParams(desc->conn_handle, 12, 12, 2, 100);
    LOG_INFO(MODULE_BLE, "App connected");
    LIGHT.turn_on_RGB(RGB_CONNECTED_MODE);
    digitalWrite(LED_BUILTIN, HIGH);

    digitalWrite(32, HIGH);
    BLE.setConnected(true);
}

void MyServerCallbacks::onDisconnect(BLEServer* pServer) {
    LOG_INFO(MODULE_BLE, "App disconnected");
    BLE.setConnected(false);
    BLE.setDownloadFlag(false);
    digitalWrite(LED_BUILTIN, LOW);
    BLE.reAdvertise();
    
};


void MessageCallbacks::onWrite(BLECharacteristic *characteristic) {
    LOG_DEBUG(MODULE_BLE, "Characteristic write received");
    std::string data = characteristic->getValue();
    String dataStr = String(data.c_str());
    
    // Reset light state for new command
    LIGHT.setLight(false);
    LIGHT.setAbleToTurnOn(true);
    
    // For Rhythm Mode
    if (dataStr.startsWith("5,")) {
        LOG_INFO(MODULE_BLE, "RHYTHM_MODE command received: %s", dataStr.c_str());
        // Force game mode change by setting to a different mode first
        DATA.setGameMode(PROCESSED_MODE);
        delay(10); // Small delay to ensure mode change is registered
        DATA.updateMsg(dataStr);
    }
    // For CONFIG_MODE
    else if (dataStr.startsWith("config:")) {
        LOG_INFO(MODULE_BLE, "CONFIG_MODE command received: %s", dataStr.c_str());
        int colonPos = dataStr.indexOf(':');
        if (colonPos > 0 && colonPos < dataStr.length() - 1) {
            int blinkCount = dataStr.substring(colonPos + 1).toInt();
            String configCmd = "100," + String(blinkCount);
            DATA.updateMsg(configCmd);
        }
    }
    // For all other modes
    else {
        LOG_INFO(MODULE_BLE, "Standard mode command received: %s", dataStr.c_str());
        // Force game mode change by setting to a different mode first
        DATA.setGameMode(PROCESSED_MODE);
        delay(10); // Small delay to ensure mode change is registered
        DATA.updateMsg(dataStr);
    }
}

void MessageCallbacks::onRead(BLECharacteristic *characteristic) {

    Serial.println("on Read");
    std::string data = characteristic->getValue();
    Serial.println(data.c_str());  }


void BluetoothControl::init(){
  // 1. Create the BLE Device
  NimBLEDevice::init(genDeviceName());
  NimBLEDevice::setMTU(517);
  
  // 2. Create the BLE server
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // 3. Create BLE Service
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  
  // 4. Create BLE Characteristics inside the service(s)
  mainCharacteristic = pService->createCharacteristic(CHARACTERISTIC_MSG_UUID, NIMBLE_PROPERTY:: READ | NIMBLE_PROPERTY:: NOTIFY | NIMBLE_PROPERTY:: WRITE);
  mainCharacteristic->setValue("checc");
  mainCharacteristic->setCallbacks(new MessageCallbacks());

  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_TX_UUID,NIMBLE_PROPERTY:: NOTIFY);

  // OTA disabled - uncomment below to enable OTA
  // pOtaCharacteristic = pService->createCharacteristic(CHARACTERISTIC_OTA_UUID,NIMBLE_PROPERTY:: WRITE | NIMBLE_PROPERTY:: WRITE_NR);
  // pOtaCharacteristic->setCallbacks(new otaCallback(pTxCharacteristic));

  // 5. Start the service(s)
  pService->start();

  // 6. Start advertising
  advertisement = pServer->getAdvertising();
  advertisement->addServiceUUID(pService->getUUID());
  advertisement->start();
  
  NimBLEDevice::startAdvertising();

  BLE.setDownloadFlag(false);
}

void BluetoothControl::reAdvertise(){
    LOG_INFO(MODULE_BLE, "Readvertising BLE services");
    advertisement->start(); 
};

void BluetoothControl::sendMsgAndNotify(String message){
    LOG_DEBUG(MODULE_BLE, "Sending message: %s", message.c_str());
    mainCharacteristic->setValue(message);
    mainCharacteristic->notify();
};


bool BluetoothControl::getConnected(){return deviceConnected;};
void BluetoothControl::setConnected(bool val){deviceConnected = val;};

bool BluetoothControl::getDownloadFlag(){return BLE_OTA_CB.getDownloadFlag();};
void BluetoothControl::setDownloadFlag(bool val){BLE_OTA_CB.setDownloadFlag(val);};

std::string BluetoothControl::genDeviceName(){
  char uniqueID[5];
  snprintf(uniqueID, 5, "%llX", ESP.getEfuseMac());
  std::string device_name = DEVICE_NAME;
  device_name += "-";
  device_name += uniqueID;

  return device_name;
};