#ifndef DATACONTROL_H
#define DATACONTROL_H

#include "global_var.h"
#include <Arduino.h>
#include "StringSplitter.h"
#include "LightControl.h"

class DataControl {
    private:
        // Standard mode variables
        int gameMode;
        int blinkBreak;
        int timedBreak;
        int buzzer;
        int buzzerTime;
        int buffer;
        int dualTof;
        int doubleModeIndex;
        //process used to tell the percentage of time remained for that exercises, egm time limit is 60, the time at that moment is 36, process is 36/60 = 16
        int process;

        // MMWave related variables
        int mmWaveStrength;
        int mmWaveDistance;
        int mmWaveDelay;

        // CONFIG_MODE variable
        int configBlinkCount; // Number of blinks for CONFIG_MODE

        // Rhythm mode specific variables
        int redValue;
        int greenValue;
        int blueValue;
        int sensorMode;

    public:
        DataControl();
        void init();

        int prevGameMode = -1;
        
        // Standard getters
        int getGameMode();
        int getBlinkBreak();
        int getTimedBreak();
        int getBuzzer();
        int getBuzzerTime();
        int getDualTof();
        int getBuffer();
        int getDoubleModeIndex();
        int getProcess();
        int getMmWaveStrength();
        int getMmWaveDistance();
        int getMmWaveDelay();
        int getConfigBlinkCount(); // Getter for configBlinkCount

        // Rhythm mode getters
        int getRedValue();
        int getGreenValue();
        int getBlueValue();
        int getSensorMode();

        // Standard setters
        void setGameMode(int val);
        void setBlinkBreak(int val);
        void setTimedBreak(int val);
        void setBuzzer(int val);
        void setBuzzerTime(int val);
        void setDoubleModeIndex(int val);
        void setBuffer(int val);
        void setConfigBlinkCount(int val); // Setter for configBlinkCount
        void setMmWaveStrength(int val);
        void setMmWaveDistance(int val);
        void setMmWaveDelay(int val);

        // Rhythm mode setters
        void setRedValue(int val);
        void setGreenValue(int val);
        void setBlueValue(int val);
        void setSensorMode(int val);

        bool isGameOn();
        void updateMsg(String data);
};

extern DataControl DATA;

#endif
