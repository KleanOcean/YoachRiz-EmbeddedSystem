#ifndef LIGHTCONTROL_H
#define LIGHTCONTROL_H

#include "Global_VAR.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "dataControl.h"
#include "BluetoothControl.h"
#include "Pangodream_18650_CL.h"
#include "MMWave.h"

#include "LightPid.h"

// Animation state structure for non-blocking animations
struct AnimationState {
    bool isRunning;
    unsigned long startTime;
    unsigned long duration;
    int currentStep;
    int totalSteps;
    unsigned long lastUpdateTime;
    int color[3];  // RGB color for animation
};

class LightControl{

    private:

        Adafruit_NeoPixel* strip_addr;
        bool lightState = false;
        uint8_t lightIntensity = 255;
        bool ableToTurnOn = false;
        unsigned long lightTurnOnTime;
        bool buzzerActive;
        unsigned long buzzerStartTime;

        //time to delay will add up
        void emitSlowly(int colour[],int time);
        void emitRandomly(int time);

        void randomWipe() ;
        void manualWipe();
        void timedWipe();
        void doubleWipe();
        void rhythmWipe();
        void restWipe();

        bool lightTurnedOn;
        bool ableToTurnOff;
        bool lightTurnedOnRGB;
        bool ableToTurnOffRGB;
        
        // Non-blocking buzzer control
        unsigned long _buzzerStartTime;
        bool _buzzerActive;
        unsigned long _buzzerDuration;

        // Countdown timer variables for Rhythm Mode
        bool countdownActive;
        unsigned long countdownStartTime;
        unsigned long countdownDuration;

        // Animation state for non-blocking TIMED mode
        AnimationState timedAnimation;
        unsigned long lastBLEProgressTime;

        // Helper methods for non-blocking animations
        void initTimedAnimation(int initialColor[3], unsigned long duration, int pixelCount);
        void updateTimedAnimation();

    public:

        LightControl();
        
        void init(bool rgb);
        //turn on RGB and Buzzer
        void turnLightON();
        void turnLightOff();
        // Non-blocking update function that should be called periodically.
        // This method handles turning off the buzzer (and optionally the LED)
        // after the configured time has elapsed.
        void update();

        bool getAbleToTurnOn();
        bool isLightTurnedOn();

        void setLight(bool val);
        void setAbleToTurnOn(bool val);
        void setAbleToTurnOff(bool val);

        void turn_on_RGB(int mode);
        void turn_off_RGB();
        void clear_light();

        void init_lighting();
        void connectedWipe();
        void configNumberWipe(int configNumber); // it is to show the config number on the light
        
        void skyblue_light();

        void opening_light();
        void closing_light();


        void emit(int colour[],int time,bool opening,bool buzzer,bool dual_led);

        // New function to set light intensity
        void setLightIntensity(uint8_t intensity);

        uint8_t getCurrentIntensity();

        void updateBuzzer(); // New method to handle buzzer state in update loop

        // Public method to abort TIMED animation
        void abortTimedAnimation();

};


extern LightControl LIGHT;

// extern MotorWheel motorWheel1;
// extern MotorWheel motorWheel2;
// extern MotorsControl MOTORS;
#endif