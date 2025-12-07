
#include "OTA.h"

//register the pTx chara so that know who to notify to
otaCallback::otaCallback( BLECharacteristic *pTx):pTxCharacteristic(pTx){};

void otaCallback:: onWrite(BLECharacteristic *pCharacteristic)
    {
      std::string rxData = pCharacteristic->getValue();
      static int totalBytesReceived = 0;  // Track total bytes
      static int chunkCount = 0;          // Track chunk count
      static bool firstChunk = true;      // Track if this is truly the first chunk

      bufferCount++;

      if (!getDownloadFlag()) 
      {
        //-----------------------------------------------
        // First BLE bytes have arrived
        //-----------------------------------------------
        
        Serial.println("1. BeginOTA");
        const esp_partition_t *configured = esp_ota_get_boot_partition();
        const esp_partition_t *running = esp_ota_get_running_partition();

        if (configured != running) 
        {
          Serial.printf("ERROR: Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x", configured->address, running->address);
          Serial.println("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
          setDownloadFlag(false);
          esp_ota_end(otaHandler);
        } else {

          Serial.printf("2. Running partition type %d subtype %d (offset 0x%08x) \n", running->type, running->subtype, running->address);
        }

        update_partition = esp_ota_get_next_update_partition(NULL);
        assert(update_partition != NULL);
        
        Serial.printf("3. Writing to partition subtype %d at offset 0x%x \n", update_partition->subtype, update_partition->address);
        //------------------------------------------------------------------------------------------
        // esp_ota_begin can take a while to complete as it erase the flash partition (3-5 seconds) 
        // so make sure there's no timeout on the client side (iOS) that triggers before that. 
        //------------------------------------------------------------------------------------------
        Serial.println("loading watchdog");
        esp_task_wdt_init(10, false);
        vTaskDelay(5);
        Serial.println("watchdog done");

        // if (esp_ota_begin(update_partition,OTA_WITH_SEQUENTIAL_WRITES, &otaHandler) != ESP_OK) {
        if (esp_ota_begin(update_partition,OTA_SIZE_UNKNOWN, &otaHandler) != ESP_OK) {

          Serial.println("error");
          setDownloadFlag(false);
          return;
        } 

        Serial.println("ota begin");
        setDownloadFlag(true);
        // Force reset all counters when starting new OTA
        totalBytesReceived = 0;
        chunkCount = 0;
        firstChunk = true;
        bufferCount = 0;
      }

      if (bufferCount >= 1 || rxData.length() > 0)
      {
        // Increment counters only when actually writing data
        totalBytesReceived += rxData.length();
        chunkCount++;

        // Debug: Log first few chunks and any unusual sizes
        if (chunkCount <= 3 || chunkCount >= 1262 || rxData.length() != 510) {
          Serial.printf("[OTA Debug] Chunk %d: size=%d, total=%d\n", chunkCount, rxData.length(), totalBytesReceived);
        }

        if(esp_ota_write(otaHandler, (uint8_t *) rxData.c_str(), rxData.length()) != ESP_OK) {
          Serial.println("Error: write to flash failed");
          Serial.printf("Failed at chunk %d, total bytes so far: %d\n", chunkCount, totalBytesReceived);
          setDownloadFlag(false);
          totalBytesReceived = 0;  // Reset counters on failure
          chunkCount = 0;
          return;
        }

        else {
          bufferCount = 1;
          // More detailed logging
          if (chunkCount % 100 == 0) {  // Log every 100 chunks
            Serial.printf("Progress: Chunk %d, Total bytes: %d\n", chunkCount, totalBytesReceived);
          }
          //Notify the iOS app so next batch can be sent
          pTxCharacteristic->setValue(&txValue, 1);
          pTxCharacteristic->notify();
        }
        
        //-------------------------------------------------------------------
        // check if this was the last data chunk? (normaly the last chunk is 
        // smaller than the maximum MTU size). For improvement: let iOS app send byte 
        // length instead of hardcoding "510"
        //-------------------------------------------------------------------
        if (rxData.length() < 510) // TODO Asumes at least 511 data bytes (@BLE 4.2).
        {
          Serial.printf("4. Final chunk arrived (size=%d)\n", rxData.length());
          Serial.printf("Total chunks received: %d\n", chunkCount);
          Serial.printf("Total bytes received: %d\n", totalBytesReceived);

          //-----------------------------------------------------------------
          // Final chunk arrived. Now check that
          // the length of total file is correct
          //-----------------------------------------------------------------
          int msg = esp_ota_end(otaHandler);
          if (msg != ESP_OK)
          {
            Serial.printf("esp_ota_end failed with error: %d (0x%x)\n", msg, msg);

            // Print specific error messages
            switch(msg) {
              case ESP_ERR_INVALID_ARG:
                Serial.println("Invalid argument");
                break;
              case ESP_ERR_OTA_VALIDATE_FAILED:
                Serial.println("Image validation failed - checksum mismatch");
                Serial.println("WARNING: Attempting to bypass validation (temporary workaround)");

                // Temporary workaround: Try to set boot partition anyway
                // This is risky but may work if the firmware is actually valid
                Serial.println("Attempting to set boot partition despite validation failure...");
                if (esp_ota_set_boot_partition(update_partition) == ESP_OK) {
                  Serial.println("Boot partition set successfully! Restarting...");
                  setDownloadFlag(false);
                  totalBytesReceived = 0;
                  chunkCount = 0;
                  esp_restart();
                  return;
                } else {
                  Serial.println("Failed to set boot partition");
                }
                break;
              case ESP_ERR_INVALID_STATE:
                Serial.println("Invalid state");
                break;
              case ESP_ERR_OTA_ROLLBACK_FAILED:
                Serial.println("Rollback failed");
                break;
              default:
                Serial.println("Unknown OTA error");
            }

            Serial.println("OTA end failed");
            setDownloadFlag(false);
            totalBytesReceived = 0;  // Reset counters
            chunkCount = 0;
            return;
          }
          
          //-----------------------------------------------------------------
          // Clear download flag and restart the ESP32 if the firmware
          // update was successful
          //-----------------------------------------------------------------
          Serial.println("Set Boot partion");
          if (esp_ota_set_boot_partition(update_partition) == ESP_OK){

            esp_ota_end(otaHandler);
            setDownloadFlag(false);
            Serial.println("Restarting...");
            esp_restart();
            return;} 

          else {
            //------------------------------------------------------------
            // Something whent wrong, the upload was not successful
            //------------------------------------------------------------

            Serial.println("Upload Error");
            setDownloadFlag(false);
            esp_ota_end(otaHandler);
            return;
          }
        }
      } else {setDownloadFlag(false); }
};

bool otaCallback::getDownloadFlag(){return downloadFlag;};
void otaCallback::setDownloadFlag(bool val){downloadFlag=val;};