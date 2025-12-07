#ifndef BLUETOOTHCONTROL_H
#define BLUETOOTHCONTROL_H

#include <NimBLEDevice.h>
#include "Global_VAR.h"
#include "LightControl.h"
#include "DataControl.h"
#include "OTA.h"

//Server that handles BLE connection and disconnection 
class MyServerCallbacks: public BLEServerCallbacks {
    
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc);
    void onDisconnect(BLEServer* pServer); 
    
};

//game msg call back
class MessageCallbacks : public BLECharacteristicCallbacks{
    //when ios app write sth, the subsequence action
    void onWrite(BLECharacteristic *characteristic);
    void onRead(BLECharacteristic *characteristic);
};

class BluetoothControl{

  private:

    BLEServer* pServer = NULL;
    BLECharacteristic *mainCharacteristic = NULL;
    BLECharacteristic *pTxCharacteristic = NULL;
    BLECharacteristic *pOtaCharacteristic = NULL;
    BLEAdvertising *advertisement = NULL;

    MyServerCallbacks BLE_Server_CB = MyServerCallbacks();
    otaCallback BLE_OTA_CB = otaCallback(pTxCharacteristic);

    bool deviceConnected = false;
    std::string genDeviceName();
    
  public:
  
    void init();
    void reAdvertise();
    //send msg to ios app and notify them
    void sendMsgAndNotify(String message);
    bool getConnected();
    void setConnected(bool val);
    //set/get the otaCallbacl otaCB to trigger internal functions
    bool getDownloadFlag();
    void setDownloadFlag(bool val);

};

extern BluetoothControl BLE;
#endif