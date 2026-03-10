// Obelisk Puzzle Code for Teensy 4.1
#include <ParagonMQTT.h>
#include <HX711.h> // Required for load cell functionality

const char *roomID = "Pharaohs";
const char *deviceID = "ObeliskHorus";

const int POWERLED = 13;

// Load Cell Pins
#define CLK 3
#define DAT 2

// Number of times a weight is recorded before it is solved
#define MEASURMENTS_RECORDED 5 // Increased for better averaging

// Variables
long rawReading = 0;
long rawAverage = 0;
char result[16];             // Buffer for result string
#define SMOOTHING_FACTOR 0.5 // Reduced for faster response (0.0-1.0)

// Custom mapping parameters (adjust based on your raw ADC range)
#define MIN_RAW_VALUE -8388608 // 24-bit ADC minimum value
#define MAX_RAW_VALUE 8388607  // 24-bit ADC maximum value
#define OUTPUT_MIN 0           // Minimum output value
#define OUTPUT_MAX 16777200    // Maximum output value for high resolution

HX711 scale;

// Function to map raw ADC values to custom range with higher precision
float mapRawValue(long rawValue, long minRaw, long maxRaw, float outputMin, float outputMax)
{
    // Constrain raw value to expected range
    if (rawValue < minRaw)
        rawValue = minRaw;
    if (rawValue > maxRaw)
        rawValue = maxRaw;

    // Map to output range
    return outputMin + (float)(rawValue - minRaw) * (outputMax - outputMin) / (float)(maxRaw - minRaw);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Obelisk Load Cell Puzzle for Horus");
    // Set Pin Modes

    pinMode(POWERLED, OUTPUT);
    digitalWrite(POWERLED, HIGH);

    // Scale
    scale.begin(DAT, CLK);
    // No calibration needed - we'll use raw ADC values
    delay(100); // Allow HX711 to initialize

    // Setup the ethernet network connection
    networkSetup();
    mqttSetup();
    delay(1000);
}

void loop()
{
    rawReading = scale.read_average(MEASURMENTS_RECORDED); // Get raw ADC value

    // Apply simple exponential smoothing
    rawAverage = (SMOOTHING_FACTOR * rawAverage) + ((1.0 - SMOOTHING_FACTOR) * rawReading);

    delay(50); // Give HX711 time to settle between readings

    // Map raw ADC value to custom range for object identification
    float mappedValue = mapRawValue(rawAverage, MIN_RAW_VALUE, MAX_RAW_VALUE, OUTPUT_MIN, OUTPUT_MAX);

    dtostrf(mappedValue, 8, 0, result); // 0 decimal places for precision
    sprintf(publishDetail, "%s", result);

    sendDataMQTT(); // ParagonMQTT library handles timing internally
}
