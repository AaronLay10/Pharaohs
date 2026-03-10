#include <FastLED.h>
#include <ParagonMQTT.h>
#include <TeensyID.h>

#define LED_PIN_1     25
#define LED_PIN_2     26
#define LED_PIN_3     27
#define LED_PIN_4     28
#define LED_PIN_5     29
#define LED_PIN_6     30
#define LED_PIN_7     31
#define LED_PIN_8     32
#define POWER_LED     13

#define COLOR_ORDER     GRB
#define CHIPSET         WS2812B
#define NUM_LEDS        3
#define NUM_STRIPS      8
#define FRAME_INTERVAL  42   // ms (~24 fps)

// ParagonMQTT device identity
const char *deviceID = "SnakeSconces";

// Candle color profile per LED position (0=base, 2=tip)
// Hue:  warm yellow-amber at base, shifting toward orange-red at tip
// Sat:  less saturated base (more white/yellow), more saturated tip
const uint8_t CANDLE_HUE[NUM_LEDS] = { 38, 22, 10 };
const uint8_t CANDLE_SAT[NUM_LEDS] = { 190, 220, 245 };

// Brightness range for each LED position
const uint8_t BRI_MIN[NUM_LEDS] = { 150, 90,  40  };
const uint8_t BRI_MAX[NUM_LEDS] = { 255, 210, 160 };

CRGB ledsA[NUM_LEDS];
CRGB ledsB[NUM_LEDS];
CRGB ledsC[NUM_LEDS];
CRGB ledsD[NUM_LEDS];
CRGB ledsE[NUM_LEDS];
CRGB ledsF[NUM_LEDS];
CRGB ledsG[NUM_LEDS];
CRGB ledsH[NUM_LEDS];

CRGB *strips[NUM_STRIPS] = { ledsA, ledsB, ledsC, ledsD, ledsE, ledsF, ledsG, ledsH };

// Current and target brightness for each strip/LED
uint8_t brCurrent[NUM_STRIPS][NUM_LEDS];
uint8_t brTarget[NUM_STRIPS][NUM_LEDS];

// ON by default; server sends "ON" or "OFF" to toggle
bool sconcesOn = true;

unsigned long previousFrameMillis = 0;

// ---------------------------------------------------------------------------
// MQTT command handlers
// ---------------------------------------------------------------------------
void handleOn(const char *) {
  sconcesOn = true;
  publish("STATE:ON");
  Serial.println("Sconces ON");
}

void handleOff(const char *) {
  sconcesOn = false;
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    fill_solid(strips[s], NUM_LEDS, CRGB::Black);
  }
  FastLED.show();
  publish("STATE:OFF");
  Serial.println("Sconces OFF");
}

// ---------------------------------------------------------------------------
// Flicker helpers
// ---------------------------------------------------------------------------
uint8_t newTarget(uint8_t pos) {
  return random8(BRI_MIN[pos], BRI_MAX[pos]);
}

// Advance one strip toward its target brightnesses, pick new targets when reached.
void updateStrip(uint8_t s) {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t step = random8(8, 35);
    if (brCurrent[s][i] < brTarget[s][i]) {
      brCurrent[s][i] = qadd8(brCurrent[s][i], step);
      if (brCurrent[s][i] >= brTarget[s][i]) brTarget[s][i] = newTarget(i);
    } else {
      brCurrent[s][i] = qsub8(brCurrent[s][i], step);
      if (brCurrent[s][i] <= brTarget[s][i]) brTarget[s][i] = newTarget(i);
    }
    brCurrent[s][i] = constrain(brCurrent[s][i], BRI_MIN[i], BRI_MAX[i]);
    strips[s][i] = CHSV(CANDLE_HUE[i], CANDLE_SAT[i], brCurrent[s][i]);
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  FastLED.addLeds<CHIPSET, LED_PIN_1, COLOR_ORDER>(ledsA, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_2, COLOR_ORDER>(ledsB, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_3, COLOR_ORDER>(ledsC, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_4, COLOR_ORDER>(ledsD, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_5, COLOR_ORDER>(ledsE, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_6, COLOR_ORDER>(ledsF, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_7, COLOR_ORDER>(ledsG, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<CHIPSET, LED_PIN_8, COLOR_ORDER>(ledsH, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);

  pinMode(POWER_LED, OUTPUT);
  digitalWrite(POWER_LED, HIGH);

  // Seed flicker at random phases so strips don't all start in sync
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      brCurrent[s][i] = random8(BRI_MIN[i], BRI_MAX[i]);
      brTarget[s][i]  = newTarget(i);
    }
  }

  // Network + MQTT
  networkSetup();
  mqttSetup();
  delay(1000);

  registerAction("ON",  handleOn);
  registerAction("OFF", handleOff);

  Serial.println("SnakeSconces ready");
}

// ---------------------------------------------------------------------------
void loop() {
  mqttBroker();

  unsigned long now = millis();
  if (now - previousFrameMillis >= FRAME_INTERVAL) {
    previousFrameMillis = now;

    if (sconcesOn) {
      for (uint8_t s = 0; s < NUM_STRIPS; s++) {
        updateStrip(s);
      }
      FastLED.show();
    }
  }
}


