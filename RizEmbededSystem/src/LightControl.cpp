#include "LightControl.h"
#include "LightPid.h"

#include <vector>

int colourPalePink[] =        {  250, 218, 221 };
int colourPink[] =          {  255,182,193  };
int colourPurple[] =        {  125,15,52  };
int colourGreen[] =         {  0,255,0  };
int colourYellow[] =        {  255,140,0  };
int colourSkyBlue[] =       {  0,255,255  };
int colourWhite[] =         {  255,255,255 };
int colourPaleGreen[] =     {  0,155,0  };
int colourCherryRed[] =     {  121,6,4 };
int colourPaleBlue[] =      { 209,231,242 };
int colourDeepGreen[] =     { 3,75,3 };
int colourTennis[] =        { 198,237,44 };
int colourOrange[] =        { 229, 75, 0 };
int colourDeepBlue[] =      { 0, 0, 178};
int bratColour[] = {138, 207, 0}; // Assigning "Cherry Red" as bratColour


TaskHandle_t lightMC;

// Define the PID controller
PID lightPID(0.2, 0.01, 0.05); // Adjust gains as needed

// Function to map intensity from [100, 7000] to [100, 1]
int mapIntensity(int input) {
    if (input <= 100) return 100;
    if (input >= 7000) return 1;

    // Define key points for piecewise linear mapping
    struct Point {
        int input;
        int output;
    };

    Point points[] = {
        {100, 100},
        {150, 95},
        {300, 70},
        {1000, 50},
        {3000, 30},
        {5000, 20},
        {7000, 10}
    };

    // Find the segment where the input falls
    for (int i = 0; i < sizeof(points) / sizeof(points[0]) - 1; ++i) {
        if (input >= points[i].input && input <= points[i + 1].input) {
            // Linear interpolation
            float slope = (float)(points[i + 1].output - points[i].output) / 
                          (points[i + 1].input - points[i].input);
            int interpolatedValue = points[i].output + slope * (input - points[i].input);

            // Round to the nearest multiple of 10
            return ((interpolatedValue + 5) / 10) * 10;
        }
    }

    return 1; // Default return value if something goes wrong
}

int reverseLogMap(int signalStrength) {
    // Define the base and scale for the logarithmic mapping
    const float base = 10.0;
    const float scale = 255.0; // Scale to the maximum intensity

    // Calculate the reverse logarithmic intensity
    // Ensure signalStrength is at least 1 to avoid log(0)
    float logValue = log10(signalStrength + 1);
    int intensity = static_cast<int>(scale / logValue);

    // Constrain the intensity to a reasonable range
    return constrain(intensity, 0, 255);
}

int mapSignalStrengthToIntensity(int signalStrength) {
    // Define key points for piecewise linear mapping
    struct Point {
        int signalStrength;
        int intensity;
    };

    Point points[] = {
        {200, 255},
        {400, 180},
        {800, 120},
        {2000, 50}
    };

    // Handle edge cases
    if (signalStrength <= points[0].signalStrength) return points[0].intensity;
    if (signalStrength >= points[3].signalStrength) return points[3].intensity;

    // Find the segment where the signal strength falls
    for (int i = 0; i < sizeof(points) / sizeof(points[0]) - 1; ++i) {
        if (signalStrength >= points[i].signalStrength && signalStrength <= points[i + 1].signalStrength) {
            // Linear interpolation
            float slope = (float)(points[i + 1].intensity - points[i].intensity) / 
                          (points[i + 1].signalStrength - points[i].signalStrength);
            return points[i].intensity + slope * (signalStrength - points[i].signalStrength);
        }
    }

    return 0; // Default return value if something goes wrong
}

LightControl::LightControl():
        strip_addr(new Adafruit_NeoPixel(LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800)) {
    pinMode(RGB_LED_PIN, OUTPUT);
    pinMode(led_pin, OUTPUT);
    pinMode(buzzer_pin, OUTPUT);
    strip_addr->setBrightness(lightIntensity);
    
    lightTurnedOn = false;
    ableToTurnOn = true;
    
    // Initialize buzzer control variables
    _buzzerActive = false;
    _buzzerStartTime = 0;
    _buzzerDuration = 0;
    
    // Initialize countdown timer variables
    countdownActive = false;
    countdownStartTime = 0;
    countdownDuration = 0;

    // Initialize animation state
    timedAnimation.isRunning = false;
    timedAnimation.startTime = 0;
    timedAnimation.duration = 0;
    timedAnimation.currentStep = 0;
    timedAnimation.totalSteps = 0;
    timedAnimation.lastUpdateTime = 0;
    timedAnimation.color[0] = 0;
    timedAnimation.color[1] = 0;
    timedAnimation.color[2] = 0;
    lastBLEProgressTime = 0;
}

void LightControl::turnLightON() {
    lightState = true;
    lightTurnOnTime = millis();

    // For Rhythm Mode, check if buzzer/timer is disabled (value = 0)
    if (DATA.getGameMode() == RHYTHM_MODE) {
        int timerValue = DATA.getBuzzerTime();
        
        if (timerValue > 0) {
            // Start buzzer using non-blocking approach
            _buzzerActive = true;
            _buzzerStartTime = millis();
            _buzzerDuration = timerValue;
            digitalWrite(buzzer_pin, HIGH);
            LOG_INFO(MODULE_LIGHT, "Buzzer activated for %d ms", timerValue);
        } else {
            // Buzzer disabled for Rhythm Mode
            LOG_INFO(MODULE_LIGHT, "Buzzer disabled for Rhythm Mode");
        }
    }
    // Handle other modes with original logic
    else if (DATA.getGameMode() != TERMINATE_MODE && DATA.getBuzzer() == TURNED_ON && DATA.getGameMode() != RESTTIMESUP_MODE) {    
        _buzzerActive = true;
        _buzzerStartTime = millis();
        _buzzerDuration = DATA.getBuzzerTime(); 
        digitalWrite(buzzer_pin, HIGH);
    }

    LOG_INFO(MODULE_LIGHT, "Turning light ON, mode: %d", DATA.getGameMode());
    digitalWrite(led_pin, HIGH);
    turn_on_RGB(DATA.getGameMode());

    DATA.prevGameMode = DATA.getGameMode();
}

void LightControl::turnLightOff() {
    lightState = false;
    ableToTurnOn = false;

    digitalWrite(led_pin,LOW);
    clear_light();

    // Ensure buzzer is turned off
    if (digitalRead(buzzer_pin) == HIGH) {
        digitalWrite(buzzer_pin, LOW);
        _buzzerActive = false;
        LOG_DEBUG(MODULE_LIGHT, "Buzzer turned off in turnLightOff()");
    }
};



bool LightControl::isLightTurnedOn() {
    return lightState;
}

void LightControl::setLightIntensity(uint8_t intensity) {
    lightIntensity = intensity;
    if (strip_addr) {
        strip_addr->setBrightness(lightIntensity);
        strip_addr->show();
    }
}

uint8_t LightControl::getCurrentIntensity() {
    return lightIntensity;
}

bool LightControl::getAbleToTurnOn() {
    return ableToTurnOn;
}

void LightControl::setAbleToTurnOn(bool able) {
    ableToTurnOn = able;
}

void LightControl::setLight(bool val){   ableToTurnOn = val;};

void LightControl::turn_on_RGB(int mode){
    static int lastMode = -1;

    // Force color logging when mode changes
    bool modeChanged = (mode != lastMode);

    switch(mode){
        case MANUAL_MODE:              manualWipe(); break;
        case RANDOM_MODE:              randomWipe(); break;
        case TIMED_MODE:               timedWipe(); break;
        case DOUBLE_MODE:              doubleWipe(); break;
        case RHYTHM_MODE:              rhythmWipe(); break;
        case OPENING_MODE:             opening_light(); break;
        case CLOSING_MODE:             closing_light(); break;
        case TERMINATE_MODE:           turnLightOff(); break;
        case RESTTIMESUP_MODE:         restWipe(); break;
        case RGB_INIT_MODE:            init_lighting(); break;
        case RGB_CONNECTED_MODE:       connectedWipe(); break;
    }

    lastMode = mode;
}

void LightControl::clear_light(){

    strip_addr->clear();
    strip_addr->show(); 
}

void LightControl::emit(int colour[],int time,bool opening = false,bool buzzer = true, bool dual_led = true){
    // For each pixel in strip...
    if (buzzer && DATA.getBuzzer()==TURNED_ON) { digitalWrite(buzzer_pin,HIGH); }
    int current = millis();

    uint32_t color = strip_addr->Color(colour[0],colour[1],colour[2]) ;

    uint32_t color2;
    if (dual_led){
        color2 = strip_addr->Color(colour[2],colour[0],colour[1]) ;}
    else{
        color2 = strip_addr->Color(colour[0],colour[1],colour[2]) ;
    }

    for(int i=0; i<strip_addr->numPixels()/2; i++) { 

        if (!lightState){   clear_light();  return;   } ;

        // uint32_t color2 = strip_addr->Color((colour[0]+80)%255,(colour[1]+80)%255,(colour[2]+80)%255) ;
        //  Set pixel's color (in RAM)
        strip_addr->setPixelColor(i, color);   
        strip_addr->setPixelColor(i+strip_addr->numPixels()/2, color2);         
        //  Update strip to match
        strip_addr->show();                          
        delay(time);  
        if (opening && DATA.getBuzzer()==TURNED_ON && digitalRead(buzzer_pin) ){
            digitalWrite(buzzer_pin,LOW); 
        }
  }
}

void LightControl::emitRandomly(int time){

    int addedUPTime = time;
    for(int i=0; i<strip_addr->numPixels()/2; i++) { 

        uint32_t color = strip_addr->Color(random(0,256),random(0,256),random(0,256)) ;
        //  Set pixel's color (in RAM)
        strip_addr->setPixelColor(i, color);    
        strip_addr->setPixelColor(i+strip_addr->numPixels()/2, color);         
        //  Update strip to match
        strip_addr->show();                   
        delay(addedUPTime);  
        addedUPTime += time;
  }
}

void LightControl::emitSlowly(int colour[],int time){
      // For each pixel in strip...
    int addedUPTime = time;
    for(int i=0; i<strip_addr->numPixels()/2; i++) { 

        uint32_t color = strip_addr->Color(colour[0],colour[1],colour[2]) ;
        //  Set pixel's color (in RAM)
        strip_addr->setPixelColor(i, color);    
        strip_addr->setPixelColor(i+strip_addr->numPixels()/2, color);          
        //  Update strip to match
        strip_addr->show();                          
        delay(addedUPTime);  
        addedUPTime += time;
  }
}


void LightControl::init_lighting() {
    // Get battery percentage
    int batteryPercentage = BL.getRawPercentage(BL.pinRead());
    int num_LED_to_show = strip_addr->numPixels();
    
    // Add debug logging
    LOG_INFO(MODULE_LIGHT, "Battery level: %d%%, Lighting %d LEDs", batteryPercentage, num_LED_to_show);

    uint32_t color;

    if (num_LED_to_show % 2 != 0){
        num_LED_to_show += 1;
    }

    // Change loop to include 4 themes
    for(int theme = 0; theme < 1; theme++) {
        // Initialize the color variables for each theme
        std::vector<int> startR(num_LED_to_show/2);
        std::vector<int> startG(num_LED_to_show/2);
        std::vector<int> startB(num_LED_to_show/2);

        for(int i=0; i<num_LED_to_show/2; i++) { 
            int color_select = random(0, 6);
            switch(theme) {
                case 0: // Green theme (original)
                    switch(color_select) {
                        case 0: // Brat Green
                            startR[i] = random(120, 150);
                            startG[i] = random(190, 220);
                            startB[i] = random(0, 10);
                            break;
                        case 1: // Yellow-Green
                            startR[i] = random(140, 170);
                            startG[i] = random(210, 240);
                            startB[i] = random(0, 5);
                            break;
                        case 2: // Neon Yellow-Green
                            startR[i] = random(180, 210);
                            startG[i] = random(230, 255);
                            startB[i] = random(0, 5);
                            break;
                        case 3: // Dark Brat Green
                            startR[i] = random(100, 130);
                            startG[i] = random(150, 190);
                            startB[i] = random(0, 10);
                            break;
                        case 4: // Olive Green
                            startR[i] = random(110, 140);
                            startG[i] = random(170, 200);
                            startB[i] = random(0, 10);
                            break;
                        case 5: // Vibrant Green
                            startR[i] = random(100, 140);
                            startG[i] = random(200, 255);
                            startB[i] = random(0, 10);
                            break;
                    }
                    break;

                case 1: // Blue theme
                    switch(color_select) {
                        case 0: // Deep Blue
                            startR[i] = random(0, 10);
                            startG[i] = random(0, 10);
                            startB[i] = random(200, 255);
                            break;
                        case 1: // Sky Blue
                            startR[i] = random(0, 10);
                            startG[i] = random(150, 200);
                            startB[i] = random(200, 255);
                            break;
                        case 2: // Turquoise
                            startR[i] = random(0, 10);
                            startG[i] = random(200, 255);
                            startB[i] = random(200, 255);
                            break;
                        case 3: // Navy
                            startR[i] = random(0, 10);
                            startG[i] = random(0, 50);
                            startB[i] = random(100, 150);
                            break;
                        case 4: // Royal Blue
                            startR[i] = random(20, 40);
                            startG[i] = random(50, 100);
                            startB[i] = random(180, 255);
                            break;
                        case 5: // Electric Blue
                            startR[i] = random(0, 20);
                            startG[i] = random(100, 150);
                            startB[i] = random(230, 255);
                            break;
                    }
                    break;

                case 2: // Orange-Red theme
                    switch(color_select) {
                        case 0: // Deep Orange
                            startR[i] = random(230, 255);
                            startG[i] = random(60, 90);
                            startB[i] = random(0, 10);
                            break;
                        case 1: // Bright Orange
                            startR[i] = random(255, 255);
                            startG[i] = random(120, 170);
                            startB[i] = random(0, 5);
                            break;
                        case 2: // Red-Orange
                            startR[i] = random(255, 255);
                            startG[i] = random(40, 80);
                            startB[i] = random(0, 5);
                            break;
                        case 3: // Coral
                            startR[i] = random(240, 255);
                            startG[i] = random(90, 130);
                            startB[i] = random(90, 130);
                            break;
                        case 4: // Burnt Orange
                            startR[i] = random(200, 230);
                            startG[i] = random(80, 120);
                            startB[i] = random(0, 10);
                            break;
                        case 5: // Fire Orange
                            startR[i] = random(255, 255);
                            startG[i] = random(100, 140);
                            startB[i] = random(0, 20);
                            break;
                    }
                    break;

                case 3: // Red theme
                    switch(color_select) {
                        case 0: // Deep Red
                            startR[i] = random(200, 255);
                            startG[i] = random(0, 20);
                            startB[i] = random(0, 20);
                            break;
                        case 1: // Ruby Red
                            startR[i] = random(155, 185);
                            startG[i] = random(0, 15);
                            startB[i] = random(0, 15);
                            break;
                        case 2: // Crimson
                            startR[i] = random(220, 255);
                            startG[i] = random(20, 40);
                            startB[i] = random(60, 80);
                            break;
                        case 3: // Wine Red
                            startR[i] = random(140, 160);
                            startG[i] = random(0, 10);
                            startB[i] = random(0, 10);
                            break;
                        case 4: // Cherry Red
                            startR[i] = random(200, 230);
                            startG[i] = random(0, 30);
                            startB[i] = random(20, 40);
                            break;
                        case 5: // Scarlet
                            startR[i] = random(230, 255);
                            startG[i] = random(30, 50);
                            startB[i] = random(0, 20);
                            break;
                    }
                    break;
            }

            color = strip_addr->Color(startR[i], startG[i], startB[i]);
            strip_addr->setPixelColor(i, color);
            strip_addr->setPixelColor(num_LED_to_show-i-1, color);
            strip_addr->show();
            delay(RGB_DISPLAY_TIME/(num_LED_to_show));
        }

        // Create gradual transitions for current theme
        for(int n=0; n<0; n++) {
            int i = random(5, num_LED_to_show/2 - 5);
            for(int offset = -3; offset <= 3; offset++) {
                int pixel = i + offset;
                for(int step = 0; step < 70; step++) {
                    int r = startR[pixel-1] + (startR[pixel] - startR[pixel-1]) * step / 100;
                    int g = startG[pixel-1] + (startG[pixel] - startG[pixel-1]) * step / 100;
                    int b = startB[pixel-1] + (startB[pixel] - startB[pixel-1]) * step / 100;
                    color = strip_addr->Color(r, g, b);

                    int num_pixels = random(1, 6);
                    for(int p=0; p<num_pixels; p++) {
                        int update_pixel = pixel + p;
                        if(update_pixel < num_LED_to_show) {
                            strip_addr->setPixelColor(update_pixel, color);
                            strip_addr->setPixelColor(num_LED_to_show-update_pixel-1, color);
                        }
                    }
                    strip_addr->show();
                }
            }
            delay(10);
        }

        // Change hold time to 7 seconds
        // delay(7000);
        
        // Optional: fade out current theme
        for(int i=0; i<num_LED_to_show; i++) {
            strip_addr->setPixelColor(i, 0);
        }
        strip_addr->show();
        delay(500); // Short delay between themes
    }

    turnLightOff();
}

void LightControl::connectedWipe() {
    LOG_DEBUG(MODULE_LIGHT, "Running on core: %d", xPortGetCoreID());
    LOG_DEBUG(MODULE_LIGHT, "Connected animation started");
    int num_LED_to_show = LED_COUNT;

    for(int i=0; i<num_LED_to_show/2; i++) { 

        uint32_t color = strip_addr->Color(colourTennis[0],colourTennis[1],colourTennis[2]) ;
        strip_addr->setPixelColor(i, color);   
        // strip_addr->setPixelColor(i+num_LED_to_show/2, color);     
        strip_addr->show();         
        delay(RGB_DISPLAY_TIME/(num_LED_to_show/2));
        }

   delay(RGB_DISPLAY_TIME);

    turnLightOff();
}

void LightControl::manualWipe() {
    // Don't control buzzer directly here - it's handled by turnLightON() and update()
    static int lastProcess = -1;
    static int lastLoggedMode = -1;
    int currentProcess = DATA.getProcess();
    int currentMode = DATA.getGameMode();

    // Force logging if mode just changed to MANUAL_MODE
    bool shouldLog = (currentProcess != lastProcess) || (currentMode != lastLoggedMode);

    if(shouldLog) {
        if(currentProcess > 50){
            LOG_INFO(MODULE_LIGHT, "Manual Mode Color: Pale Blue (RGB: %d,%d,%d), Process: %d",
                     colourPaleBlue[0], colourPaleBlue[1], colourPaleBlue[2], currentProcess);
            emit(colourPaleBlue,0,false,false);
        }
        else if (currentProcess > 25){
            LOG_INFO(MODULE_LIGHT, "Manual Mode Color: Sky Blue (RGB: %d,%d,%d), Process: %d",
                     colourSkyBlue[0], colourSkyBlue[1], colourSkyBlue[2], currentProcess);
            emit(colourSkyBlue,0,false,false);
        }
        else{
            LOG_INFO(MODULE_LIGHT, "Manual Mode Color: Deep Blue (RGB: %d,%d,%d), Process: %d",
                     colourDeepBlue[0], colourDeepBlue[1], colourDeepBlue[2], currentProcess);
            emit(colourDeepBlue,0,false,false);
        }
        lastProcess = currentProcess;
        lastLoggedMode = currentMode;
    } else {
        // Still emit the color but don't log
        if(currentProcess > 50){
            emit(colourPaleBlue,0,false,false);
        }
        else if (currentProcess > 25){
            emit(colourSkyBlue,0,false,false);
        }
        else{
            emit(colourDeepBlue,0,false,false);
        }
    }
}

void LightControl::doubleWipe() {
    static int lastDoubleIndex = -1;
    static int lastLoggedMode = -1;
    int currentDoubleIndex = DATA.getDoubleModeIndex();
    int currentMode = DATA.getGameMode();

    // Force logging if mode just changed to DOUBLE_MODE
    bool shouldLog = (currentDoubleIndex != lastDoubleIndex) || (currentMode != lastLoggedMode);

    if (shouldLog) {
        if (currentDoubleIndex == 0) {
            LOG_INFO(MODULE_LIGHT, "Double Mode Color: Orange (RGB: %d,%d,%d)",
                     colourOrange[0], colourOrange[1], colourOrange[2]);
            emit(colourOrange,0,false,true,false);
        } else {
            LOG_INFO(MODULE_LIGHT, "Double Mode Color: Deep Blue (RGB: %d,%d,%d)",
                     colourDeepBlue[0], colourDeepBlue[1], colourDeepBlue[2]);
            emit(colourDeepBlue,0,false,true,false);
        }
        lastDoubleIndex = currentDoubleIndex;
        lastLoggedMode = currentMode;
    } else {
        // Still emit the color but don't log
        if (currentDoubleIndex == 0) {
            emit(colourOrange,0,false,true,false);
        } else {
            emit(colourDeepBlue,0,false,true,false);
        }
    }
}

void LightControl::randomWipe() {
    // Always use neon green for RANDOM mode
    uint32_t neonGreenColor = strip_addr->Color(57, 255, 20);  // Neon green (RGB: 57, 255, 20)

    // Second color for the other half of the strip (keeping this from the original code)
    uint32_t color2 = strip_addr->Color(120, 120, 120);

    LOG_INFO(MODULE_LIGHT, "Random Mode Color: Neon Green (RGB: 57,255,20)");

    // Light up LEDs with neon green
    int between_led_waiting_time = 5;
    for(int i = 0; i < strip_addr->numPixels()/2; i++) {
        if (!lightState) {
            clear_light();
            return;
        }

        // Set pixel's color in RAM
        strip_addr->setPixelColor(i, neonGreenColor);
        strip_addr->setPixelColor(i + strip_addr->numPixels()/2, color2);

        // Update strip to match
        strip_addr->show();
        delay(between_led_waiting_time);
    }
    


//     uint32_t color;
//     uint32_t color2 = strip_addr->Color( 120,120,120);;

//     if(DATA.getProcess() > 50){ color = strip_addr->Color( 0,255,255); }
//     else if (DATA.getProcess() > 25){ color = strip_addr->Color(255,255,0) ; }
//     else{  color = strip_addr->Color(255,0,255) ;}

//     int between_led_waiting_time = 5;
//     for(int i=0; i<strip_addr->numPixels()/2; i++) { 
//         if (!lightState){   clear_light();  return;   } ;
//         // uint32_t color = strip_addr->Color(random(0,256),random(0,256),random(0,256)) ;
//         //  Set pixel's color (in RAM)
//         strip_addr->setPixelColor(i, color);  
//         strip_addr->setPixelColor(i+strip_addr->numPixels()/2, color2);         
//         //  Update strip to match
//         strip_addr->show();                          
//         delay(between_led_waiting_time);  
//   }
}

void LightControl::timedWipe() {
    // Determine color based on process value
    int* animationColor = colourDeepBlue;  // Default
    if (DATA.getProcess() > 50) {
        animationColor = colourPaleBlue;
    } else if (DATA.getProcess() > 25) {
        animationColor = colourOrange;
    }

    // Fill all LEDs with the selected color first
    // First layer: LED 0-23, Second layer: LED 24-47
    for (int i = 0; i < strip_addr->numPixels(); i++) {
        uint32_t color = strip_addr->Color(animationColor[0], animationColor[1], animationColor[2]);
        strip_addr->setPixelColor(i, color);
    }
    strip_addr->show();

    // Initialize non-blocking animation state
    unsigned long duration = DATA.getTimedBreak();
    int pixelCount = strip_addr->numPixels() / 2;
    initTimedAnimation(animationColor, duration, pixelCount);

    LOG_INFO(MODULE_LIGHT, "TIMED mode started: %lu ms duration, RGB(%d,%d,%d)",
             duration, animationColor[0], animationColor[1], animationColor[2]);
};

//three loops, one sky blue, one pale yellow, one bright red, with 100ms delay at each light blub
void LightControl::opening_light(){
    // 12*83 ~= 1000s
    int time = RGB_DISPLAY_TIME/(LED_COUNT/2);
    // TF_Luna.takeBaseline();
    emit(colourDeepBlue,time,true);
    emit(colourPaleBlue,time,true);
    emit(colourDeepBlue,time,true);
    turnLightOff();
};


//blink three times
void LightControl::closing_light(){

    for(int i=0; i<3; i++) {
        //since the waiting time is 0, didnt turn off buzzer
        emit(colourCherryRed, 0);
        delay(100);
        if(DATA.getBuzzer()==TURNED_ON && digitalRead(buzzer_pin)){ digitalWrite(buzzer_pin,LOW);}
        delay(300);
        clear_light();
        delay(300);}

    turnLightOff();
};

void LightControl::restWipe(){

    int eachRestTime = DATA.getBlinkBreak()/(strip_addr->numPixels()/2);
    strip_addr->setBrightness(RGB_REST_INTENSITY);

    emit(colourTennis,0,false,false);

    for(int i=0; i<strip_addr->numPixels()/2; i++) { 

        delay(eachRestTime);      
        if (!lightState){   clear_light();  return;   } ;
        // For each pixel in strip...
        uint32_t color = strip_addr->Color(0,0,0) ;
        strip_addr->setPixelColor(i, color);  //  Set pixel's color (in RAM)
        strip_addr->setPixelColor(i+strip_addr->numPixels()/2, color);  //  Set pixel's color (in RAM)
        strip_addr->show();       
    }

    strip_addr->setBrightness(RGB_INTENSITY);
    turnLightOff();

    //alert the user to ready
    digitalWrite(buzzer_pin,HIGH);
    delay(100);
    digitalWrite(buzzer_pin,LOW);

};

void LightControl::init(bool initLight){
        strip_addr->begin();
        strip_addr->clear();  // INITIALIZE NeoPixel strip object (REQUIRED)
        strip_addr->show();  // Turn OFF all pixels ASAP
        // strip_addr->setBrightness(RGB_INTENSITY);

        //  xTaskCreatePinnedToCore(MultiThreadLightOn,"MultiThreadLightOn",5000,NULL,3,&lightMC,1);


        // Buzz once at startup - 1 second
        // digitalWrite(buzzer_pin,HIGH);
        // delay(1000);
        // digitalWrite(buzzer_pin,LOW);

        if(initLight){turn_on_RGB(RGB_INIT_MODE); }

};

/**
 * Initialize TIMED mode animation state
 * @param initialColor RGB color array [R, G, B]
 * @param duration Total animation duration in milliseconds
 * @param pixelCount Total number of pixels to animate
 */
void LightControl::initTimedAnimation(int initialColor[3], unsigned long duration, int pixelCount) {
    timedAnimation.isRunning = true;
    timedAnimation.startTime = millis();
    timedAnimation.duration = duration;
    timedAnimation.currentStep = 0;
    timedAnimation.totalSteps = pixelCount;
    timedAnimation.lastUpdateTime = timedAnimation.startTime;
    timedAnimation.color[0] = initialColor[0];
    timedAnimation.color[1] = initialColor[1];
    timedAnimation.color[2] = initialColor[2];
    lastBLEProgressTime = timedAnimation.startTime;

    LOG_INFO(MODULE_LIGHT, "TIMED animation initialized: %lums duration, %d steps, RGB(%d,%d,%d)",
             duration, pixelCount, initialColor[0], initialColor[1], initialColor[2]);
}

/**
 * Update TIMED mode animation state (non-blocking)
 * Calculates which LED should be off based on elapsed time
 */
void LightControl::updateTimedAnimation() {
    if (!timedAnimation.isRunning) return;

    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - timedAnimation.startTime;

    // Animation complete - send final countdown message
    if (elapsedTime >= timedAnimation.duration) {
        abortTimedAnimation();
        LOG_INFO(MODULE_LIGHT, "TIMED animation completed");

        // Send overtime/completion message only when animation finishes
        if (BLE.getConnected()) {
            BLE.sendMsgAndNotify("timed_countdown:0");
            LOG_INFO(MODULE_LIGHT, "TIMED animation overtime - sent completion message");
        }

        turnLightOff();
        return;
    }

    // Calculate which step we should be at based on elapsed time
    int targetStep = (elapsedTime * timedAnimation.totalSteps) / timedAnimation.duration;

    // Update LED pixels if we've moved to a new step
    if (targetStep > timedAnimation.currentStep) {
        for (int i = timedAnimation.currentStep; i < targetStep && i < timedAnimation.totalSteps; i++) {
            uint32_t blackColor = strip_addr->Color(0, 0, 0);
            // First layer: turn off sequentially from LED 0 to LED 23
            strip_addr->setPixelColor(i, blackColor);
            // Second layer: turn off in reverse from LED 47 down to LED 24
            int secondLayerIndex = strip_addr->numPixels() - 1 - i;
            strip_addr->setPixelColor(secondLayerIndex, blackColor);
        }
        strip_addr->show();
        timedAnimation.currentStep = targetStep;
        timedAnimation.lastUpdateTime = currentTime;
    }

    // No periodic messages during animation - only send on completion or detection
}

/**
 * Abort TIMED mode animation
 */
void LightControl::abortTimedAnimation() {
    timedAnimation.isRunning = false;
    timedAnimation.currentStep = 0;
    LOG_INFO(MODULE_LIGHT, "TIMED animation aborted");
}

// Non-blocking update function to be called repeatedly
void LightControl::update() {
    const unsigned long MIN_LOOP_TIME = 3;
    unsigned long loopStartTime = millis();

    // Update TIMED mode animation
    if (timedAnimation.isRunning) {
        updateTimedAnimation();
    }

    // Update buzzer state
    if (_buzzerActive && (millis() - _buzzerStartTime >= _buzzerDuration)) {
        digitalWrite(buzzer_pin, LOW);
        _buzzerActive = false;
        LOG_DEBUG(MODULE_LIGHT, "Buzzer turned off after %lu ms", _buzzerDuration);
    }

    // Handle countdown timer for Rhythm Mode
    if (countdownActive && (millis() - countdownStartTime >= countdownDuration)) {
        turnLightOff();
        countdownActive = false;
        LOG_INFO(MODULE_LIGHT, "Countdown timer expired, turning off light");
    }

    // Calculate loop timing
    unsigned long loopTime = millis() - loopStartTime;
    if (loopTime < MIN_LOOP_TIME) {
        delay(MIN_LOOP_TIME - loopTime);
    }
}

void LightControl::configNumberWipe(int configNumber) {
    LOG_INFO(MODULE_LIGHT, "Configuring number wipe: %d", configNumber);
    
    // Just to show to user where is the device
    uint32_t color;
    for(int i=0; i<LED_COUNT; i++) {
        color = strip_addr->Color(255, 255, 255); 
        strip_addr->setPixelColor(i, color);
        // Set brightness to max
        strip_addr->setBrightness(255);
        strip_addr->show();
    }
    
    // Start buzzer without blocking
    digitalWrite(buzzer_pin, HIGH);
    
    // Store the time when buzzer was turned on
    _buzzerStartTime = millis();
    _buzzerActive = true;
    _buzzerDuration = 400; // 0.5 seconds
    
    LOG_INFO(MODULE_LIGHT, "Buzzer activated for 500ms (non-blocking)");
}

void LightControl::rhythmWipe() {
    // Get RGB values and parameters
    int redValue = DATA.getRedValue();
    int greenValue = DATA.getGreenValue();
    int blueValue = DATA.getBlueValue();
    int timerValue = DATA.getTimedBreak();
    int buzzerValue = DATA.getBuzzerTime();
    int sensorMode = DATA.getSensorMode();

    // Track last color to only log on change
    static int lastRed = -1, lastGreen = -1, lastBlue = -1;
    static int lastLoggedMode = -1;
    int currentMode = DATA.getGameMode();

    // Force logging if mode just changed to RHYTHM_MODE or color changed
    bool colorChanged = (redValue != lastRed || greenValue != lastGreen || blueValue != lastBlue)
                        || (currentMode != lastLoggedMode);

    // Create color using the RGB values
    uint32_t customColor = strip_addr->Color(redValue, greenValue, blueValue);

    if (colorChanged) {
        LOG_INFO(MODULE_LIGHT, "Rhythm Mode Color: RGB(%d,%d,%d), Timer: %d ms, Sensor: %d",
                 redValue, greenValue, blueValue, timerValue, sensorMode);
        lastRed = redValue;
        lastGreen = greenValue;
        lastBlue = blueValue;
        lastLoggedMode = currentMode;
    }

    // Light up all LEDs with the custom color
    for(int i = 0; i < strip_addr->numPixels(); i++) {
        strip_addr->setPixelColor(i, customColor);
    }
    strip_addr->show();

    // Handle timer countdown if enabled (timerValue > 0)
    if (timerValue > 0) {
        countdownActive = true;
        countdownStartTime = millis();
        countdownDuration = timerValue;
        if (colorChanged) {
            LOG_INFO(MODULE_LIGHT, "Countdown timer started: %d ms", timerValue);
        }
    } else {
        countdownActive = false;
        if (colorChanged) {
            LOG_INFO(MODULE_LIGHT, "No timer set, light will stay on until sensor detection");
        }
    }
}