// Obelisk Puzzle Code for Teensy 4.1
#include <ParagonMQTT.h>
#include <HX711.h> // Required for load cell functionality

const char *roomID = "Pharaohs";
const char *deviceID = "Test Benchy";

#define calibration_factor 7050.0 // This value is obtained using the SparkFun_HX711_Calibration sketch

const int POWERLED = 13;

// MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;

// Load Cell Pins
#define CLK 15
#define DAT 14

// To MAGLOCK Drop Panel
#define MAGLOCK 9

// Number of times a weight is recorded before it is solved
#define MEASURMENTS_RECORDED 5 // Increased for better averaging

// Variables
float weight = 0;
float weightAverage = 0;
char result[8];
#define SMOOTHING_FACTOR 0.5 // Reduced for faster response (0.0-1.0)

HX711 scale;

void setup()
{
  Serial.begin(115200);
  Serial.println("Obelisk Load Cell Puzzle for Anubis");
  // Set Pin Modes
  pinMode(MAGLOCK, OUTPUT);
  pinMode(POWERLED, OUTPUT);
  digitalWrite(MAGLOCK, LOW);
  digitalWrite(POWERLED, HIGH);

  // Scale
  scale.begin(DAT, CLK);
  scale.set_scale(calibration_factor);
  delay(100);   // Allow HX711 to initialize
  scale.tare(); // Assuming there is no weight on the scale at start up, reset the scale to 0

  // Setup the ethernet network connection
  networkSetup();
  mqttSetup();
  delay(1000);
  scale.tare();

  // Register MQTT actions
  registerAction("Reset", [](const char *)
                 { onResetOne(); });
  registerAction("DropPanel", [](const char *)
                 { onDropPanel(); });
  registerAction("tare", [](const char *)
                 { tare(); });
}

void loop()
{
  weight = scale.get_units(MEASURMENTS_RECORDED);

  // Apply simple exponential smoothing
  weightAverage = (SMOOTHING_FACTOR * weightAverage) + ((1.0 - SMOOTHING_FACTOR) * weight);

  delay(50); // Give HX711 time to settle between readings

  unsigned long currentMillisMqtt = millis();
  if (currentMillisMqtt - previousMillisMqtt > MQTTDELAY)
  {
    previousMillisMqtt = currentMillisMqtt;
    float displayWeight = (weightAverage < 0) ? 0 : weightAverage;
    dtostrf(displayWeight, 6, 1, result);
    sprintf(publishDetail, "%s", result);
    Serial.println(result);
  }
  sendDataMQTT();
}

void onResetOne()
{
  digitalWrite(MAGLOCK, LOW);
  sprintf(publishDetail, "CONNECTED");
}

void onDropPanel()
{
  digitalWrite(MAGLOCK, HIGH);
  Serial.println("Drop Panel");
  sprintf(publishDetail, "DROPPANEL");
}

void tare()
{
  scale.tare();
  Serial.println("TARE!!");
  sprintf(publishDetail, "TARE");
}
