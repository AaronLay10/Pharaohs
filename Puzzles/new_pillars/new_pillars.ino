/******************************************************************************
 * PHARAOHS ESCAPE ROOM — NEW PILLARS CONTROLLER
 *
 * Single firmware for all 4 Egyptian fire pillars. Auto-detects which pillar
 * it's running on via the Teensy's MAC address — compile once, flash to all.
 *
 * Uses FastLED for WS2812 LED output and color math/animation.
 *
 * HARDWARE PER PILLAR:
 *   - 4 sides (A, B, C, D) x 2 LED strips x 72 LEDs = 144 LEDs per side
 *   - 1 bowl strip x 32 LEDs on top of pillar
 *   - 12 IR sensors (3 positions x 4 sides)
 *   - Raspberry Pi audio controller (Serial7 on pins 28/29)
 *   - NativeEthernet + MQTT communication
 *
 * STATES (driven by MQTT):
 *   State 1 — WAITING:  Dark, idle, waiting for game server
 *   State 2 — HANDS:    Green glow on handprint sensors
 *   State 3 — PILLAR:   Pillar-colored section fire on active sensors
 *   State 4 — SOLVED:   Full dramatic fire on all sides + bowl
 *
 * PIN NOTE:
 *   Original pins 40/41 rewired to 22/23 (FastLED max pin = 39).
 ******************************************************************************/

#include <Arduino.h>
#include <FastLED.h>
#include <TeensyID.h>
#include <ParagonMQTT.h>

#include "pillar_config.h"
#include "fire_engine.h"
#include "sound_engine.h"

// ─── Runtime Pillar Identity (set in setup) ─────────────────────────────────
uint8_t           pillarIndex = 0;
const char*       deviceID    = "PillarOne";
const SensorInfo* sensorMap   = SENSOR_MAPS[0];
PillarHue         pillarHue   = {1, 0, 0};

// ─── FastLED CRGB Arrays ────────────────────────────────────────────────────
CRGB strips[NUM_STRIPS][LEDS_PER_STRIP];

#define BRIGHTNESS            255
#define FRAMES_PER_SECOND     60

// ─── State Machine ──────────────────────────────────────────────────────────
typedef uint8_t puzzleState_t;
enum {
  WAITING_STATE,
  HANDS_STATE,
  PILLAR_STATE,
  SOLVED_STATE,
};

static puzzleState_t puzzle_state = WAITING_STATE;
static bool sensors[NUMBER_OF_SENSORS];

// ─── Forward Declarations ───────────────────────────────────────────────────
void showLeds();
void setupLeds();
void onState1();
void onState2();
void onState3();
void onState4();
void renderWaiting();
void renderHands(uint32_t now);
void renderPillar(uint32_t now);
void renderSolved(uint32_t now);
void setPixelOnSide(uint8_t side, uint16_t index, CRGB color);
void setPixelOnBowl(uint16_t index, CRGB color);
void blackoutAll();
void clearSensors();
void readSensors();

// ─── MQTT Action Handlers ───────────────────────────────────────────────────
void actionState1(const char *value) { (void)value; onState1(); }
void actionState2(const char *value) { (void)value; onState2(); }
void actionState3(const char *value) { (void)value; onState3(); }
void actionState4(const char *value) { (void)value; onState4(); }

// ═══════════════════════════════════════════════════════════════════════════
// LED SETUP — Register strips with FastLED based on detected pillar
// ═══════════════════════════════════════════════════════════════════════════
//
// FastLED requires compile-time template pins, so we use a switch on
// pillarIndex to register the correct pin configuration.

void setupLeds()
{
  switch(pillarIndex)
  {
    case 0: // Pillar 1: pins 5, 6, 22, 23, 18, 17, 7, 11, 12
      FastLED.addLeds<WS2812, 5,  GRB>(strips[0], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 6,  GRB>(strips[1], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 22, GRB>(strips[2], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 23, GRB>(strips[3], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 18, GRB>(strips[4], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 17, GRB>(strips[5], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 7,  GRB>(strips[6], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 11, GRB>(strips[7], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 12, GRB>(strips[8], LEDS_PER_STRIP);
      break;

    case 1: // Pillar 2: pins 17, 21, 6, 5, 11, 10, 23, 16, 12
      FastLED.addLeds<WS2812, 17, GRB>(strips[0], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 21, GRB>(strips[1], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 6,  GRB>(strips[2], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 5,  GRB>(strips[3], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 11, GRB>(strips[4], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 10, GRB>(strips[5], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 23, GRB>(strips[6], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 16, GRB>(strips[7], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 12, GRB>(strips[8], LEDS_PER_STRIP);
      break;

    case 2: // Pillar 3: pins 6, 5, 23, 22, 18, 21, 10, 7, 12
      FastLED.addLeds<WS2812, 6,  GRB>(strips[0], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 5,  GRB>(strips[1], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 23, GRB>(strips[2], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 22, GRB>(strips[3], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 18, GRB>(strips[4], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 21, GRB>(strips[5], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 10, GRB>(strips[6], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 7,  GRB>(strips[7], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 12, GRB>(strips[8], LEDS_PER_STRIP);
      break;

    case 3: // Pillar 4: pins 5, 6, 10, 11, 18, 17, 23, 22, 12
      FastLED.addLeds<WS2812, 5,  GRB>(strips[0], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 6,  GRB>(strips[1], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 10, GRB>(strips[2], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 11, GRB>(strips[3], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 18, GRB>(strips[4], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 17, GRB>(strips[5], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 23, GRB>(strips[6], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 22, GRB>(strips[7], LEDS_PER_STRIP);
      FastLED.addLeds<WS2812, 12, GRB>(strips[8], LEDS_PER_STRIP);
      break;
  }

  FastLED.setBrightness(BRIGHTNESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// LED OUTPUT
// ═══════════════════════════════════════════════════════════════════════════

void showLeds()
{
  FastLED.show();
}

// ═══════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════

void setup()
{
  Serial.begin(115200);
  delay(500);

  pinMode(POWER_LED_PIN, OUTPUT);
  digitalWrite(POWER_LED_PIN, HIGH);

  // ── Auto-detect which pillar we are ─────────────────────────────────────
  pillarIndex = detectPillar();
  deviceID    = PILLAR_NAMES[pillarIndex];
  sensorMap   = SENSOR_MAPS[pillarIndex];
  pillarHue   = PILLAR_HUES[pillarIndex];

  Serial.printf("Running as: %s (Pillar %d)\n", deviceID, pillarIndex + 1);
  Serial.printf("Hue: R=%d G=%d B=%d\n", pillarHue.r_scale, pillarHue.g_scale, pillarHue.b_scale);

  // ── Set up LED strips via FastLED ─────────────────────────────────────
  setupLeds();
  blackoutAll();

  // ── Initialize sensors ──────────────────────────────────────────────────
  for(uint8_t i = 0; i < NUMBER_OF_SENSORS; i++)
  {
    pinMode(sensorMap[i].pin, INPUT_PULLUP);
  }
  clearSensors();

  // ── Initialize subsystems ───────────────────────────────────────────────
  fireEngineInit();
  soundEngineInit();

  // ── Network + MQTT ──────────────────────────────────────────────────────
  networkSetup();
  mqttSetup();
  registerAction("state1", actionState1);
  registerAction("state2", actionState2);
  registerAction("state3", actionState3);
  registerAction("state4", actionState4);
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════════════════════════

void loop()
{
  uint32_t now = millis();

  switch(puzzle_state)
  {
    case WAITING_STATE:
      renderWaiting();
      snprintf(publishDetail, sizeof(publishDetail), "State 1 = Waiting");
      break;

    case HANDS_STATE:
      readSensors();
      renderHands(now);
      break;

    case PILLAR_STATE:
      readSensors();
      renderPillar(now);
      break;

    case SOLVED_STATE:
      renderSolved(now);
      break;
  }

  // Publish sensor telemetry
  if(puzzle_state != WAITING_STATE)
  {
    snprintf(publishDetail, sizeof(publishDetail),
      "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
      sensors[0], sensors[1], sensors[2],  sensors[3],
      sensors[4], sensors[5], sensors[6],  sensors[7],
      sensors[8], sensors[9], sensors[10], sensors[11]);
  }

  // Always service MQTT
  sendDataMQTT();

  // Frame rate control
  delay(1000 / FRAMES_PER_SECOND);
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE RENDERERS
// ═══════════════════════════════════════════════════════════════════════════

// ── State 1: WAITING ────────────────────────────────────────────────────────
void renderWaiting()
{
  // All dark — nothing to render
}

// ── State 2: HANDS ──────────────────────────────────────────────────────────
void renderHands(uint32_t now)
{
  (void)now;

  for(uint8_t i = 0; i < NUMBER_OF_SENSORS; i++)
  {
    uint8_t side = sensorMap[i].side_index;
    uint8_t pos  = sensorMap[i].position_index;
    uint16_t sec = sensorMap[i].led_section_start;

    if(sensorMap[i].handprint_solution)
    {
      if(sensors[i] == IR_ACTIVATED)
        glow_brightness[side][pos] = qadd8(glow_brightness[side][pos], GLOW_FADE_IN_RATE);
      else
        glow_brightness[side][pos] = qsub8(glow_brightness[side][pos], GLOW_FADE_OUT_RATE);
    }

    CRGB color = CRGB(0, glow_brightness[side][pos], 0);
    for(uint16_t j = sec; j < sec + LEDS_PER_PANEL; j++)
    {
      setPixelOnSide(side, j, color);
    }
  }

  showLeds();
}

// ── State 3: PILLAR ─────────────────────────────────────────────────────────
void renderPillar(uint32_t now)
{
  bool ignite_active[NUM_IGNITE_SOUNDS];
  memset(ignite_active, 0, sizeof(ignite_active));

  const uint16_t section_starts[NUMBER_OF_PANELS] = {
    BOTTOM_SECTION_START, MIDDLE_SECTION_START, TOP_SECTION_START
  };

  for(uint8_t side = 0; side < NUMBER_OF_SIDES; side++)
  {
    for(uint8_t pos = 0; pos < NUMBER_OF_PANELS; pos++)
    {
      uint16_t sec = section_starts[pos];

      for(uint8_t i = 0; i < NUMBER_OF_SENSORS; i++)
      {
        if(sensorMap[i].side_index != side) continue;
        if(sensorMap[i].led_section_start != sec) continue;

        if(sensors[i] == IR_ACTIVATED)
        {
          side_heat[side][sec] = qadd8(side_heat[side][sec], random8(130, 220));
          uint8_t snd_idx = sensorToSoundIndex(side, pos);
          ignite_active[snd_idx] = true;
        }
      }

      stepFire(side_heat[side], sec, LEDS_PER_PANEL, SECTION_COOLING, SECTION_SPARKING, false);

      for(uint16_t j = sec; j < sec + LEDS_PER_PANEL; j++)
      {
        CRGB color = applyPillarTint(heatToColor(side_heat[side][j]));
        setPixelOnSide(side, j, color);
      }
    }
  }

  // Bowl fire — pillar tinted
  stepFire(bowl_heat, 0, LEDS_IN_BOWL, BOWL_COOLING, BOWL_SPARKING, true);
  ignite_active[SND_IGNITE_BOWL] = true;
  for(uint16_t a = 0; a < LEDS_IN_BOWL; a++)
  {
    CRGB color = applyPillarTint(heatToColor(bowl_heat[a]));
    setPixelOnBowl(a, color);
  }

  showLeds();
  processSounds(ignite_active, true, now);
}

// ── State 4: SOLVED ─────────────────────────────────────────────────────────
void renderSolved(uint32_t now)
{
  bool ignite_active[NUM_IGNITE_SOUNDS];
  memset(ignite_active, 0, sizeof(ignite_active));

  for(uint8_t side = 0; side < NUMBER_OF_SIDES; side++)
  {
    stepSolvedFire(side_heat[side], 0, LEDS_PER_STRIP, true);

    for(uint16_t j = 0; j < LEDS_PER_STRIP; j++)
    {
      CRGB color = heatToColor(side_heat[side][j]);
      setPixelOnSide(side, j, color);
    }

    ignite_active[SND_IGNITE_BOTTOM_A] = true;
  }

  // Bowl — dramatic fire
  stepSolvedFire(bowl_heat, 0, LEDS_IN_BOWL, true);
  ignite_active[SND_IGNITE_BOWL] = true;
  for(uint16_t a = 0; a < LEDS_IN_BOWL; a++)
  {
    CRGB color = heatToColor(bowl_heat[a]);
    setPixelOnBowl(a, color);
  }

  showLeds();
  processSounds(ignite_active, false, now);
}

// ═══════════════════════════════════════════════════════════════════════════
// LED HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void setPixelOnSide(uint8_t side, uint16_t index, CRGB color)
{
  if(index >= LEDS_PER_STRIP) return;
  uint8_t s1 = side * 2;
  uint8_t s2 = side * 2 + 1;
  strips[s1][index] = color;
  strips[s2][index] = color;
}

void setPixelOnBowl(uint16_t index, CRGB color)
{
  if(index >= LEDS_IN_BOWL) return;
  strips[STRIP_BOWL][index] = color;
}

void blackoutAll()
{
  for(uint8_t s = 0; s < NUM_STRIPS; s++)
  {
    fill_solid(strips[s], LEDS_PER_STRIP, CRGB::Black);
  }
  showLeds();
}

// ═══════════════════════════════════════════════════════════════════════════
// SENSOR HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void clearSensors()
{
  for(uint8_t i = 0; i < NUMBER_OF_SENSORS; i++)
  {
    sensors[i] = IR_NOT_ACTIVATED;
  }
}

void readSensors()
{
  for(uint8_t i = 0; i < NUMBER_OF_SENSORS; i++)
  {
    sensors[i] = digitalRead(sensorMap[i].pin);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE TRANSITIONS (called by MQTT actions)
// ═══════════════════════════════════════════════════════════════════════════

void onState1()
{
  puzzle_state = WAITING_STATE;
  clearSensors();
  blackoutAll();
}

void onState2()
{
  puzzle_state = HANDS_STATE;
  clearSensors();
  blackoutAll();
  fireEngineInit();
  mqttBroker();
  publish("CONNECTED");
}

void onState3()
{
  puzzle_state = PILLAR_STATE;
  clearSensors();
  blackoutAll();
  fireEngineInit();
}

void onState4()
{
  puzzle_state = SOLVED_STATE;
  clearSensors();
  blackoutAll();
  fireEngineInit();
}
