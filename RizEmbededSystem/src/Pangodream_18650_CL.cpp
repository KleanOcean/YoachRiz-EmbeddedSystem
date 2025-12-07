/*
  MIT License
  
  Copyright (c) 2019 Pangodream
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "Arduino.h"
#include "Pangodream_18650_CL.h"

Pangodream_18650_CL::Pangodream_18650_CL(int addressPin, double convFactor, int reads)
{
    _reads = reads;
    _convFactor = convFactor;
    _addressPin = addressPin;
    _initVoltsArray();
    
    // Initialize filter variables
    _readingIndex = 0;
    _filterFilled = false;
    _lastFilteredReading = 0;
    _lastPercentage = -1;
    
    // Initialize readings array with zeros
    for (int i = 0; i < BATTERY_FILTER_SIZE; i++) {
        _batteryReadings[i] = 0;
    }
}

Pangodream_18650_CL::Pangodream_18650_CL(int addressPin, double convFactor)
{
    _reads = DEF_READS;
    _convFactor = convFactor;
    _addressPin = addressPin;
    _initVoltsArray();
}

Pangodream_18650_CL::Pangodream_18650_CL(int addressPin)
{
    _reads = DEF_READS;
    _convFactor = DEF_CONV_FACTOR;
    _addressPin = addressPin;
    _initVoltsArray();
}

Pangodream_18650_CL::Pangodream_18650_CL()
{
    _reads = DEF_READS;
    _convFactor = DEF_CONV_FACTOR;
    _addressPin = DEF_PIN;
    _initVoltsArray();
}

int Pangodream_18650_CL::getAnalogPin()
{
    return _addressPin;
}
double Pangodream_18650_CL::getConvFactor()
{
    return _convFactor;
}
    
/**
 * Loads each voltage value in its matching charge element (index)
 */
void Pangodream_18650_CL::_initVoltsArray() {
    _vs[0] = 3.650;  // 0% SoC
    _vs[1] = 3.655; _vs[2] = 3.660; _vs[3] = 3.665; _vs[4] = 3.670; _vs[5] = 3.675;
    _vs[6] = 3.680; _vs[7] = 3.685; _vs[8] = 3.690; _vs[9] = 3.695; _vs[10] = 3.700;
    _vs[11] = 3.705; _vs[12] = 3.710; _vs[13] = 3.715; _vs[14] = 3.720; _vs[15] = 3.725;
    _vs[16] = 3.730; _vs[17] = 3.735; _vs[18] = 3.740; _vs[19] = 3.745; _vs[20] = 3.750;
    _vs[21] = 3.755; _vs[22] = 3.760; _vs[23] = 3.765; _vs[24] = 3.770; _vs[25] = 3.775;
    _vs[26] = 3.780; _vs[27] = 3.785; _vs[28] = 3.790; _vs[29] = 3.795; _vs[30] = 3.800;
    _vs[31] = 3.805; _vs[32] = 3.810; _vs[33] = 3.815; _vs[34] = 3.820; _vs[35] = 3.825;
    _vs[36] = 3.830; _vs[37] = 3.835; _vs[38] = 3.840; _vs[39] = 3.845; _vs[40] = 3.850;
    _vs[41] = 3.855; _vs[42] = 3.860; _vs[43] = 3.865; _vs[44] = 3.870; _vs[45] = 3.875;
    _vs[46] = 3.880; _vs[47] = 3.885; _vs[48] = 3.890; _vs[49] = 3.895; _vs[50] = 3.900;
    _vs[51] = 3.905; _vs[52] = 3.910; _vs[53] = 3.915; _vs[54] = 3.920; _vs[55] = 3.925;
    _vs[56] = 3.930; _vs[57] = 3.935; _vs[58] = 3.940; _vs[59] = 3.945; _vs[60] = 3.950;
    _vs[61] = 3.955; _vs[62] = 3.960; _vs[63] = 3.965; _vs[64] = 3.970; _vs[65] = 3.975;
    _vs[66] = 3.980; _vs[67] = 3.985; _vs[68] = 3.990; _vs[69] = 3.995; _vs[70] = 4.000;
    _vs[71] = 4.005; _vs[72] = 4.010; _vs[73] = 4.015; _vs[74] = 4.020; _vs[75] = 4.025;
    _vs[76] = 4.030; _vs[77] = 4.035; _vs[78] = 4.040; _vs[79] = 4.045; _vs[80] = 4.050;
    _vs[81] = 4.055; _vs[82] = 4.060; _vs[83] = 4.065; _vs[84] = 4.070; _vs[85] = 4.075;
    _vs[86] = 4.080; _vs[87] = 4.085; _vs[88] = 4.090; _vs[89] = 4.095; _vs[90] = 4.100;
    _vs[91] = 4.105; _vs[92] = 4.110; _vs[93] = 4.115; _vs[94] = 4.120; _vs[95] = 4.125;
    _vs[96] = 4.130; _vs[97] = 4.135; _vs[98] = 4.140; _vs[99] = 4.150; _vs[100] = 4.200; // 100% SoC
}

int Pangodream_18650_CL::getBatteryChargeLevel()
{
    int readValue = _analogRead(_addressPin);
    double volts = _analogReadToVolts(readValue);
    int chargeLevel = _getChargeLevel(volts);
    return chargeLevel;
}

int Pangodream_18650_CL::pinRead(){
    return _analogRead(_addressPin); 
}

int Pangodream_18650_CL::_analogRead(int pinNumber){
    int totalValue = 0;
    int averageValue = 0;
    for(int i = 0; i < _reads; i++){
       totalValue += analogRead(pinNumber);
    }
    averageValue = totalValue / _reads;
    return averageValue; 
}
/**
 * Performs a binary search to find the index corresponding to a voltage.
 * The index of the array is the charge %
*/
int Pangodream_18650_CL::_getChargeLevel(double volts){
  int idx = 50;
  int prev = 0;
  int half = 0;
  if (volts >= 4.2){
    return 100;
  }
  if (volts <= 3.2){
    return 0;
  }
  while(true){
    half = abs(idx - prev) / 2;
    prev = idx;
    if(volts >= _vs[idx]){
      idx = idx + half;
    }else{
      idx = idx - half;
    }
    if (prev == idx){
      break;
    }
  }
  return idx;
}

double Pangodream_18650_CL::_analogReadToVolts(int readValue){
  double volts; 
  volts = readValue * _convFactor / 1000;
  return volts;
}

/**
 * Maps raw ADC values to battery percentage using a comprehensive lookup table
 */
int Pangodream_18650_CL::getRawPercentage(int rawValue) {
  // Comprehensive mapping table from raw ADC values to percentage
  // Ensures that raw value 2123 is consistently reported as 44%
  static const struct {
    int adc;
    int percentage;
  } BATTERY_TABLE[] = {
    {2018, 0},   {2023, 1},   {2028, 2},   {2033, 3},   {2038, 4},   
    {2043, 5},   {2048, 6},   {2053, 8},   {2058, 10},  {2063, 12},  
    {2068, 16},  {2070, 20},  {2075, 22},  {2080, 24},  {2085, 26},  
    {2090, 28},  {2095, 30},  {2100, 33},  {2105, 36},  {2110, 38},  
    {2111, 40},  {2115, 41},  {2120, 43},  {2123, 44},  {2125, 45},  
    {2130, 48},  {2135, 51},  {2140, 53},  {2145, 56},  {2150, 58},  
    {2155, 61},  {2160, 63},  {2165, 66},  {2170, 68},  {2174, 70},  
    {2176, 71},  {2178, 72},  {2180, 73},  {2182, 74},  {2184, 75},  
    {2185, 76},  {2187, 77},  {2190, 78},  {2193, 80},  {2196, 81},  
    {2200, 83},  {2203, 85},  {2206, 86},  {2210, 87},  {2215, 89},  
    {2220, 90},  {2225, 91},  {2230, 92},  {2235, 93},  {2240, 94},  
    {2245, 95},  {2250, 96},  {2260, 97},  {2270, 98},  {2280, 98},  
    {2290, 99},  {2300, 99},  {2310, 99},  {2320, 99},  {2323, 100}
  };
  
  const int TABLE_SIZE = sizeof(BATTERY_TABLE) / sizeof(BATTERY_TABLE[0]);
  
  // Check boundary conditions
  if (rawValue <= BATTERY_TABLE[0].adc) return BATTERY_TABLE[0].percentage;
  if (rawValue >= BATTERY_TABLE[TABLE_SIZE-1].adc) return BATTERY_TABLE[TABLE_SIZE-1].percentage;
  
  // Find the right interval in the table
  for (int i = 0; i < TABLE_SIZE-1; i++) {
    if (rawValue == BATTERY_TABLE[i].adc) {
      // Exact match
      return BATTERY_TABLE[i].percentage;
    }
    else if (rawValue > BATTERY_TABLE[i].adc && rawValue < BATTERY_TABLE[i+1].adc) {
      // Falls between two entries - linear interpolation
      return BATTERY_TABLE[i].percentage + 
        ((rawValue - BATTERY_TABLE[i].adc) * 
         (BATTERY_TABLE[i+1].percentage - BATTERY_TABLE[i].percentage)) / 
        (BATTERY_TABLE[i+1].adc - BATTERY_TABLE[i].adc);
    }
  }
  
  // Should never reach here
  return 50;
}

/**
 * Maps raw ADC values to voltage using a comprehensive lookup table
 */
double Pangodream_18650_CL::getVoltageFromRaw(int rawValue) {
  // Comprehensive mapping table from raw ADC values to voltage
  static const struct {
    int adc;
    double voltage;
  } VOLTAGE_TABLE[] = {
    {2018, 3.64}, {2023, 3.65}, {2030, 3.66}, {2040, 3.67}, {2050, 3.68},
    {2060, 3.69}, {2070, 3.70}, {2080, 3.71}, {2090, 3.72}, {2100, 3.73},
    {2111, 3.74}, {2120, 3.75}, {2123, 3.76}, {2130, 3.77}, {2140, 3.78},
    {2150, 3.79}, {2160, 3.80}, {2170, 3.81}, {2174, 3.82}, {2185, 3.83},
    {2190, 3.84}, {2203, 3.85}, {2210, 3.87}, {2220, 3.89}, {2230, 3.91},
    {2240, 3.93}, {2250, 3.95}, {2260, 3.97}, {2270, 3.99}, {2280, 4.01},
    {2290, 4.04}, {2300, 4.07}, {2310, 4.09}, {2320, 4.11}, {2323, 4.12}
  };
  
  const int TABLE_SIZE = sizeof(VOLTAGE_TABLE) / sizeof(VOLTAGE_TABLE[0]);
  
  // Check boundary conditions
  if (rawValue <= VOLTAGE_TABLE[0].adc) return VOLTAGE_TABLE[0].voltage;
  if (rawValue >= VOLTAGE_TABLE[TABLE_SIZE-1].adc) return VOLTAGE_TABLE[TABLE_SIZE-1].voltage;
  
  // Find the right interval in the table
  for (int i = 0; i < TABLE_SIZE-1; i++) {
    if (rawValue == VOLTAGE_TABLE[i].adc) {
      // Exact match
      return VOLTAGE_TABLE[i].voltage;
    }
    else if (rawValue > VOLTAGE_TABLE[i].adc && rawValue < VOLTAGE_TABLE[i+1].adc) {
      // Falls between two entries - linear interpolation
      return VOLTAGE_TABLE[i].voltage + 
        ((rawValue - VOLTAGE_TABLE[i].adc) * 
         (VOLTAGE_TABLE[i+1].voltage - VOLTAGE_TABLE[i].voltage)) / 
        (VOLTAGE_TABLE[i+1].adc - VOLTAGE_TABLE[i].adc);
    }
  }
  
  // Should never reach here
  return 3.80;
}

double Pangodream_18650_CL::getBatteryVolts(){
    int readValue = _analogRead(_addressPin);
    // Use the calibrated voltage table instead of the conversion factor
    return getVoltageFromRaw(readValue);
}

/**
 * Updates the moving average filter with a new reading
 * @param newReading New raw ADC reading to add to filter
 * @return Filtered raw ADC reading
 */
int Pangodream_18650_CL::_updateFilter(int newReading) {
    // Add the new reading to the array
    _batteryReadings[_readingIndex] = newReading;
    
    // Increment index and wrap around if needed
    _readingIndex = (_readingIndex + 1) % BATTERY_FILTER_SIZE;
    
    // Set flag once we've filled the filter
    if (_readingIndex == 0) {
        _filterFilled = true;
    }
    
    // Calculate average of readings
    long sum = 0;
    int count = _filterFilled ? BATTERY_FILTER_SIZE : _readingIndex;
    
    // Ensure we don't divide by zero
    if (count == 0) {
        return newReading;
    }
    
    // Sum all valid readings
    for (int i = 0; i < count; i++) {
        sum += _batteryReadings[i];
    }
    
    // Calculate average
    int filteredReading = sum / count;
    
    // Store last filtered reading
    _lastFilteredReading = filteredReading;
    
    return filteredReading;
}

/**
 * Gets filtered, consistent battery percentage
 * @return Consistent battery percentage (0-100)
 */
int Pangodream_18650_CL::getFilteredPercentage() {
    // Read current raw value
    int rawReading = _analogRead(_addressPin);
    
    // Apply filter
    int filteredRaw = _updateFilter(rawReading);
    
    // Get percentage from filtered reading
    int currentPercentage = getRawPercentage(filteredRaw);
    
    // Apply hysteresis to prevent small fluctuations
    if (_lastPercentage != -1) {
        // Only change by 1% at a time for small changes (less than 3%)
        if (abs(currentPercentage - _lastPercentage) <= 3) {
            if (currentPercentage > _lastPercentage) {
                currentPercentage = _lastPercentage + 1;
            } else if (currentPercentage < _lastPercentage) {
                currentPercentage = _lastPercentage - 1;
            }
        }
    }
    
    // Store this percentage for next time
    _lastPercentage = currentPercentage;
    
    return currentPercentage;
}

//it should return a percentage 0 - 100
int Pangodream_18650_CL::getLED_Scale(){
  // Use the filtered percentage for consistent readings
  return getFilteredPercentage();
}