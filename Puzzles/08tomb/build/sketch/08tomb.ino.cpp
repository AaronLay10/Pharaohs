#include <Arduino.h>
#line 1 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
#include <OctoWS2811.h>
#include <ParagonMQTT.h>

// MQTT Configuration - Required by ParagonMQTT
const char *deviceID = "Tomb";
const char *roomID = "Pharaohs";

// Motor Control
#define MOTOR_UP 14
#define MOTOR_DOWN 15

// Upper Sensors
#define UPPER_RIGHT 18
#define UPPER_LEFT 21
#define UPPER_INDUCTION 16

// Lower Sensors
#define LOWER_RIGHT 19
#define LOWER_LEFT 20
#define LOWER_INDUCTION 17

#define POWER_LED 13

#define FRAMES_PER_SECOND 50

// OctoWS2811 Setup
const int numPins = 1;
byte pinList[numPins] = {22};
const int ledsPerStrip = 200;
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

int pos = 0;

int state = 0;

boolean solved = false;
static const uint8_t sensor_pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
#define NUMBER_OF_SENSOR_PINS 10

static uint32_t frame;

typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_t;

const struct
{
  uint8_t amplitude; /* 255 is max */
  uint8_t frequency;
  double velocity; /* in leds per frame */
  float midpoint;
  rgb_t color;
} wave_parameters[] =
    {
        {80, 1, 0.1, 0.0f, {255, 100, 3}},
        {140, 3, 0.1, 0.4f, {255, 0, 0}},
};
#define NUMBER_OF_WAVES (sizeof(wave_parameters) / sizeof(wave_parameters[0]))

#line 68 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void setup();
#line 106 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void loop();
#line 127 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void onSolve();
#line 133 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void onReset();
#line 140 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void onOverride();
#line 146 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void motorMove(boolean solved);
#line 219 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void update_leds(boolean enabled, uint32_t frame);
#line 68 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/08tomb/08tomb.ino"
void setup()
{
  uint8_t i;

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  Serial.begin(115200);

  leds.begin();
  leds.show();

  pinMode(UPPER_RIGHT, INPUT_PULLDOWN);
  pinMode(LOWER_RIGHT, INPUT_PULLDOWN);
  pinMode(UPPER_LEFT, INPUT_PULLDOWN);
  pinMode(LOWER_LEFT, INPUT_PULLDOWN);
  pinMode(MOTOR_UP, OUTPUT);
  pinMode(MOTOR_DOWN, OUTPUT);
  pinMode(UPPER_INDUCTION, INPUT_PULLDOWN);
  pinMode(LOWER_INDUCTION, INPUT_PULLDOWN);
  pinMode(POWER_LED, OUTPUT);

  for (i = 0; i < NUMBER_OF_SENSOR_PINS; i++)
  {
    pinMode(sensor_pins[i], INPUT_PULLUP);
  }
  digitalWrite(POWER_LED, HIGH);
  // Setup the ethernet network connection
  networkSetup();
  // Setup the MQTT service
  mqttSetup();

  // Register MQTT action callbacks
  registerAction("solved", onSolve);
  registerAction("reset", onReset);
  registerAction("override", onOverride);
}

void loop()
{
  // Update sensor data in the publishDetail buffer
  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X,%02X:%02X:%02X:%02X:%02X:%02X",
          digitalRead(sensor_pins[0]), digitalRead(sensor_pins[1]), digitalRead(sensor_pins[2]), digitalRead(sensor_pins[3]),
          digitalRead(sensor_pins[4]), digitalRead(sensor_pins[5]), digitalRead(sensor_pins[6]), digitalRead(sensor_pins[7]),
          digitalRead(sensor_pins[8]), digitalRead(sensor_pins[9]),
          digitalRead(UPPER_LEFT), digitalRead(LOWER_LEFT), digitalRead(UPPER_RIGHT), digitalRead(LOWER_RIGHT),
          digitalRead(UPPER_INDUCTION), digitalRead(LOWER_INDUCTION));

  // Let ParagonMQTT handle timing and send data when appropriate
  sendDataMQTT();

  update_leds(solved, frame);
  motorMove(solved);

  frame++;

  leds.show();
}

void onSolve()
{
  solved = true;
  Serial.println("Solved");
}

void onReset()
{
  solved = false;
  Serial.println("Reset!!");
  publish("CONNECTED");
}

void onOverride()
{
  solved = true;
  Serial.println("Override!!");
}

void motorMove(boolean solved)
{
  boolean atTheTop = false;
  boolean atTheBottom = false;

  // Check if any upper sensors are triggered (stops upward motion)
  // Physical switches are active low, induction sensors are active high
  if (digitalRead(UPPER_INDUCTION) == HIGH || digitalRead(UPPER_RIGHT) == LOW || digitalRead(UPPER_LEFT) == LOW)
  {
    atTheTop = true;
  }

  // Check if any lower sensors are triggered (stops downward motion)
  // Physical switches are active low, induction sensors are active high
  if (digitalRead(LOWER_INDUCTION) == HIGH || digitalRead(LOWER_RIGHT) == LOW || digitalRead(LOWER_LEFT) == LOW)
  {
    atTheBottom = true;
  }

  // Debug sensor readings
  Serial.print("Upper Induction = ");
  Serial.print(digitalRead(UPPER_INDUCTION));
  Serial.print(", Upper Right = ");
  Serial.print(digitalRead(UPPER_RIGHT));
  Serial.print(", Upper Left = ");
  Serial.print(digitalRead(UPPER_LEFT));
  Serial.print(" | Lower Induction = ");
  Serial.print(digitalRead(LOWER_INDUCTION));
  Serial.print(", Lower Right = ");
  Serial.print(digitalRead(LOWER_RIGHT));
  Serial.print(", Lower Left = ");
  Serial.println(digitalRead(LOWER_LEFT));
  Serial.print("atTheTop = ");
  Serial.print(atTheTop);
  Serial.print(", atTheBottom = ");
  Serial.print(atTheBottom);
  Serial.print(", solved = ");
  Serial.println(solved);

  if (solved)
  {
    // When solved, move up until any upper sensor is triggered
    if (atTheTop)
    {
      Serial.println("At the Top");
      digitalWrite(MOTOR_UP, LOW);
      digitalWrite(MOTOR_DOWN, LOW);
    }
    else
    {
      Serial.println("Motor Going Up");
      digitalWrite(MOTOR_DOWN, LOW);
      digitalWrite(MOTOR_UP, HIGH);
    }
  }
  else
  {
    // When not solved, move down until any lower sensor is triggered
    if (atTheBottom)
    {
      Serial.println("At the Bottom");
      digitalWrite(MOTOR_UP, LOW);
      digitalWrite(MOTOR_DOWN, LOW);
    }
    else
    {
      Serial.println("Motor Going Down");
      digitalWrite(MOTOR_UP, LOW);
      digitalWrite(MOTOR_DOWN, HIGH);
    }
  }
}

void update_leds(boolean enabled, uint32_t frame)
{
  uint16_t i;
  uint8_t j;
  float amplitude;
  float frequency;
  double velocity;
  float midpoint;
  rgb_t color;
  float brightness;
  float r;
  float g;
  float b;
  float x;

  for (i = 0; i < ledsPerStrip; i++)
  {
    r = 0;
    g = 0;
    b = 0;

    for (j = 0; j < NUMBER_OF_WAVES; j++)
    {
      amplitude = wave_parameters[j].amplitude;
      frequency = wave_parameters[j].frequency;
      color = wave_parameters[j].color;
      velocity = wave_parameters[j].velocity;
      midpoint = wave_parameters[j].midpoint;
      x = 2.0f * PI / (float)(ledsPerStrip - 1) * (i * frequency + (float)(velocity * (double)frame));
      x = fmod(x, 2.0f * PI);
      brightness = amplitude / 2.0f * (midpoint + cosf(x));
      brightness = max(0.0f, brightness);

      if (enabled)
      {
        r += (float)color.r / 255.0f * brightness;
        g += (float)color.g / 255.0f * brightness;
        b += (float)color.b / 255.0f * brightness;
      }
    }

    color.r = (uint8_t)r;
    color.g = (uint8_t)g;
    color.b = (uint8_t)b;

    leds.setPixel(i, color.r, color.g, color.b);
  }
}
