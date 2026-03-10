#include <Arduino.h>
#line 1 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
#include <FastLED.h>
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

// FastLED Setup
#define LED_PIN 22
#define NUM_LEDS 200
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

int pos = 0;

int state = 0;

boolean solved = false;
boolean raiseAnimationActive = false;
static const uint8_t sensor_pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 12, 11};  // Pin 10 has hardware conflicts - using pin 12 instead
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
        {200, 2, 0.08, 1.0f, {255, 0, 0}},  // Creepy red wave traveling slowly
};
#define NUMBER_OF_WAVES (sizeof(wave_parameters) / sizeof(wave_parameters[0]))

#line 64 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void setup();
#line 104 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void loop();
#line 125 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void onSolve();
#line 131 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void onReset();
#line 139 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void onOverride();
#line 145 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void motorMove(boolean solved);
#line 221 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void update_leds(boolean enabled, uint32_t frame);
#line 64 "/Users/aaron/Git Repos/Pharaohs/Puzzles/08tomb/08tomb.ino"
void setup()
{
  uint8_t i;

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  Serial.begin(115200);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();

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

  motorMove(solved);
  update_leds(raiseAnimationActive, frame);

  frame++;

  FastLED.show();
}

void onSolve()
{
  solved = true;
  Serial.println("Solved");
}

void onReset()
{
  solved = false;
  raiseAnimationActive = false;
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
    raiseAnimationActive = true;

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
    raiseAnimationActive = false;
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
  static float wispPosA = 0.0f;
  static float wispPosB = (float)NUM_LEDS * 0.55f;
  static float wispSpeedA = 0.028f;
  static float wispSpeedB = -0.022f;
  static float wispAmpA = 205.0f;
  static float wispAmpB = 130.0f;
  static uint32_t lastDriftFrame = 0;
  uint16_t sideLen = NUM_LEDS / 4;

  if (!enabled)
  {
    for (i = 0; i < NUM_LEDS; i++)
    {
      leds[i] = CRGB::Black;
    }
    return;
  }

  // Slowly mutate speed and strength so it feels haunted instead of repeating.
  if ((frame - lastDriftFrame) > 140)
  {
    lastDriftFrame = frame;

    if (random(100) < 22)
    {
      float s = ((float)random(1, 5)) / 100.0f;
      wispSpeedA = (random(100) < 78) ? s : -s;
    }

    if (random(100) < 22)
    {
      float s = ((float)random(1, 4)) / 100.0f;
      wispSpeedB = (random(100) < 72) ? -s : s;
    }

    wispAmpA = (float)random(170, 235);
    wispAmpB = (float)random(100, 165);
  }

  for (i = 0; i < NUM_LEDS; i++)
  {
    const float darkRedFloor = 18.0f;
    float red;
    float dA;
    float dB;
    float glowA;
    float glowB;
    float sidePos;
    float edgeBreath;
    float voidField;
    float cornerBias;
    float nearestCorner;
    float flicker;

    dA = fabsf((float)i - wispPosA);
    if (dA > ((float)NUM_LEDS * 0.5f))
    {
      dA = (float)NUM_LEDS - dA;
    }

    dB = fabsf((float)i - wispPosB);
    if (dB > ((float)NUM_LEDS * 0.5f))
    {
      dB = (float)NUM_LEDS - dB;
    }

    glowA = wispAmpA / (1.0f + (0.022f * dA * dA));
    glowB = wispAmpB / (1.0f + (0.030f * dB * dB));

    sidePos = (float)(i % sideLen) / (float)sideLen;
    edgeBreath = 5.0f + (6.0f * (0.5f + 0.5f * sinf((1.0f * PI * sidePos) - ((float)frame * 0.006f))));

    // Pull slightly brighter at corners to make etched panel edges feel alive.
    nearestCorner = (float)(i % sideLen);
    if (nearestCorner > ((float)sideLen * 0.5f))
    {
      nearestCorner = (float)sideLen - nearestCorner;
    }
    cornerBias = 0.84f + (0.26f * (1.0f - (nearestCorner / ((float)sideLen * 0.5f))));

    // Two opposing shadow fields create drifting black veins.
    voidField = 0.34f + (0.66f * (0.5f + 0.5f * sinf(((float)i * 0.040f) - ((float)frame * 0.038f))));
    voidField *= 0.42f + (0.58f * (0.5f + 0.5f * sinf(((float)i * 0.060f) + ((float)frame * 0.031f) + 1.7f)));

    flicker = 0.88f + ((float)random(0, 16) / 100.0f);

    // Rare hard black tear for sudden eerie pulses.
    if (random(1000) < 6)
    {
      voidField *= 0.45f;
    }

    red = (edgeBreath + glowA + glowB) * cornerBias * voidField * flicker;
    red = darkRedFloor + (red * 0.82f);

    if (red > 255.0f)
    {
      red = 255.0f;
    }
    else if (red < 0.0f)
    {
      red = 0.0f;
    }

    leds[i] = CRGB((uint8_t)red, 0, 0);
  }

  wispPosA += wispSpeedA;
  if (wispPosA >= (float)NUM_LEDS)
  {
    wispPosA -= (float)NUM_LEDS;
  }
  else if (wispPosA < 0.0f)
  {
    wispPosA += (float)NUM_LEDS;
  }

  wispPosB += wispSpeedB;
  if (wispPosB >= (float)NUM_LEDS)
  {
    wispPosB -= (float)NUM_LEDS;
  }
  else if (wispPosB < 0.0f)
  {
    wispPosB += (float)NUM_LEDS;
  }
}
