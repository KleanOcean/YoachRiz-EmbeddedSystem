#include "dataControl.h"
#include "Log.h"

// Helper function to get mode name for logging
static const char* getModeName(int mode) {
    switch(mode) {
        case MANUAL_MODE: return "MANUAL";
        case RANDOM_MODE: return "RANDOM";
        case TIMED_MODE: return "TIMED";
        case DOUBLE_MODE: return "DOUBLE";
        case RHYTHM_MODE: return "RHYTHM";
        case MOVEMENT_MODE: return "MOVEMENT";
        case OPENING_MODE: return "OPENING";
        case CLOSING_MODE: return "CLOSING";
        case TERMINATE_MODE: return "TERMINATE";
        case RESTTIMESUP_MODE: return "RESTTIMESUP";
        case PROCESSED_MODE: return "PROCESSED";
        case CONFIG_MODE: return "CONFIG";
        default: return "UNKNOWN";
    }
}

DataControl::DataControl()
    : gameMode(DEFAULT_GAMEMODE),
      blinkBreak(DEFAULT_BLINKBREAK),
      timedBreak(DEFAULT_TIMEDBREAK),
      buzzer(DEFAULT_BUZZER),
      buzzerTime(DEFAULT_BUZZERTIME),
      buffer(DEFAULT_BUFFER),
      mmWaveStrength(200),
      mmWaveDistance(20),
      mmWaveDelay(0),
      configBlinkCount(1),
      redValue(0),
      greenValue(0),
      blueValue(0),
      sensorMode(0) {
}


void DataControl::init(){
    
        //multi thread here
}

int DataControl::getGameMode(){      return gameMode;};
int DataControl::getBlinkBreak(){    return blinkBreak;};
int DataControl::getTimedBreak(){    return timedBreak;};
int DataControl::getBuzzer(){        return buzzer;};
int DataControl::getBuzzerTime(){        return buzzerTime;};
int DataControl::getBuffer(){        return buffer;};
int DataControl::getDualTof(){      return dualTof;};
int DataControl::getDoubleModeIndex(){      return doubleModeIndex;};
int DataControl::getProcess(){      return process;};
int DataControl::getMmWaveStrength(){      return mmWaveStrength;};
int DataControl::getMmWaveDistance(){      return mmWaveDistance;};
int DataControl::getMmWaveDelay(){      return mmWaveDelay;};
int DataControl::getConfigBlinkCount(){    return configBlinkCount;};
int DataControl::getRedValue() { return redValue; }
int DataControl::getGreenValue() { return greenValue; }
int DataControl::getBlueValue() { return blueValue; }
int DataControl::getSensorMode() { return sensorMode; }

void DataControl::setGameMode(int mode) {
    LOG_INFO(MODULE_DATA, "Mode transition: %s(%d) → %s(%d)",
             getModeName(gameMode), gameMode, getModeName(mode), mode);
    gameMode = mode;
}

void DataControl::setBlinkBreak(int val) { blinkBreak = val; }
void DataControl::setTimedBreak(int val) { timedBreak = val; }
void DataControl::setDoubleModeIndex(int val) { doubleModeIndex = val; }
void DataControl::setBuffer(int val) { buffer = val; }
void DataControl::setMmWaveStrength(int val) { mmWaveStrength = val; }
void DataControl::setMmWaveDistance(int val) { mmWaveDistance = val; }
void DataControl::setMmWaveDelay(int val) { mmWaveDelay = val; }
void DataControl::setConfigBlinkCount(int val) { configBlinkCount = val; }
void DataControl::setRedValue(int val) { redValue = val; }
void DataControl::setGreenValue(int val) { greenValue = val; }
void DataControl::setBlueValue(int val) { blueValue = val; }
void DataControl::setSensorMode(int val) { sensorMode = val; }
void DataControl::setBuzzer(int val) { buzzer = val; }
void DataControl::setBuzzerTime(int val) { buzzerTime = val; }

bool DataControl::isGameOn(){
    if(gameMode == MANUAL_MODE || gameMode == RANDOM_MODE || gameMode == DOUBLE_MODE || gameMode == TIMED_MODE){
        return true;
    }
    return false;
};

void DataControl::updateMsg(String data) {
    LOG_INFO(MODULE_DATA, "Updating data from message");
    LOG_DEBUG(MODULE_DATA, "Message data: %s", data.c_str());

    if (data.length() == 0) {
        LOG_ERROR(MODULE_DATA, "Empty data string received");
        return;
    }

    // Parse first field (game mode)
    int firstComma = data.indexOf(',');
    if (firstComma == -1) {
        LOG_ERROR(MODULE_DATA, "Invalid message format - no commas found");
        return;
    }

    int mode = data.substring(0, firstComma).toInt();
    LOG_INFO(MODULE_DATA, "Mode transition: %s(%d) → %s(%d)",
             getModeName(gameMode), gameMode, getModeName(mode), mode);
    
    // Force mode change by setting to different mode first
    if (mode == gameMode) {
        setGameMode(PROCESSED_MODE);
        delay(10);
    }
    setGameMode(mode);

    // Handle CONFIG_MODE specially
    if (mode == CONFIG_MODE) {
        if (firstComma < data.length() - 1) {
            int blinkCount = data.substring(firstComma + 1).toInt();
            LOG_INFO(MODULE_DATA, "CONFIG_MODE: Setting blink count to %d", blinkCount);
            setConfigBlinkCount(blinkCount);
        }
        return;
    }

    // Find all comma positions
    int commaPositions[7];
    int commaCount = 0;
    int searchPos = 0;
    
    while (commaCount < 7 && searchPos < data.length()) {
        searchPos = data.indexOf(',', searchPos + 1);
        if (searchPos == -1) break;
        commaPositions[commaCount++] = searchPos;
    }

    if (commaCount != 7) {
        LOG_ERROR(MODULE_DATA, "Invalid message format - expected 8 fields, found %d", commaCount + 1);
        return;
    }

    // Parse remaining fields based on game mode
    switch (mode) {
        case RHYTHM_MODE: {
            // Format: mode,red,green,blue,timerValue,buzzerValue,sensorMode,placeholder
            setRedValue(data.substring(commaPositions[0]+1, commaPositions[1]).toInt());
            setGreenValue(data.substring(commaPositions[1]+1, commaPositions[2]).toInt());
            setBlueValue(data.substring(commaPositions[2]+1, commaPositions[3]).toInt());
            setTimedBreak(data.substring(commaPositions[3]+1, commaPositions[4]).toInt());
            setBuzzerTime(data.substring(commaPositions[4]+1, commaPositions[5]).toInt());
            setSensorMode(data.substring(commaPositions[5]+1, commaPositions[6]).toInt());
            
            LOG_INFO(MODULE_DATA, "Rhythm Mode: RGB(%d,%d,%d), Timer=%d, Buzzer=%d, Sensor=%d",
                    getRedValue(), getGreenValue(), getBlueValue(), 
                    getTimedBreak(), getBuzzerTime(), getSensorMode());
            break;
        }
        
        default: {
            // Standard format: mode,blinkBreak,timedBreak,buzzer,buzzerTime,buffer,doubleModeIndex,process
            setBlinkBreak(data.substring(commaPositions[0]+1, commaPositions[1]).toInt());
            setTimedBreak(data.substring(commaPositions[1]+1, commaPositions[2]).toInt());
            setBuzzer(data.substring(commaPositions[2]+1, commaPositions[3]).toInt());
            setBuzzerTime(data.substring(commaPositions[3]+1, commaPositions[4]).toInt());
            setBuffer(data.substring(commaPositions[4]+1, commaPositions[5]).toInt() + 1);
            setDoubleModeIndex(data.substring(commaPositions[5]+1, commaPositions[6]).toInt());
            process = data.substring(commaPositions[6]+1).toInt();
            
            LOG_INFO(MODULE_DATA, "Standard Mode %d: Break=%d, Timer=%d, Buzzer=%d/%d, Buffer=%d, Double=%d, Process=%d",
                    mode, getBlinkBreak(), getTimedBreak(), getBuzzer(), 
                    getBuzzerTime(), getBuffer(), getDoubleModeIndex(), process);
            break;
        }
    }
}
