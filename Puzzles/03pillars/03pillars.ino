/******************************************************************************
 * PHARAOHS ESCAPE ROOM - EGYPTIAN PILLARS CONTROLLER
 * 
 * PROGRAMMING INSTRUCTIONS:
 * 1. Set PILLAR define below to 1, 2, 3, or 4
 * 2. Upload to the corresponding Teensy
 * 3. Pin assignments and MQTT deviceID will auto-configure
 * 
 * HARDWARE PER PILLAR:
 * - 4 sides (A, B, C, D) × 2 LED strips × 72 LEDs = 144 LEDs per side
 * - 12 IR sensors (3 locations × 4 sides)
 * - Raspberry Pi audio controller (Serial7 on pins 28/29)
 * - NativeEthernet + MQTT communication
 * 
 * NAMING CONVENTION:
 * - Sides: A, B, C, D
 * - Sensor Locations: T (Top), M (Middle), B (Bottom)
 * - LED Strips: 1, 2 (two per side)
 * - Example: SENSOR_AT_PIN = Side A, Top sensor
 * - Example: LED pins A1, A2 = Side A, strips 1 and 2
 * 
 * PUZZLE STATES:
 * - WAITING_STATE: Idle until the server advances this pillar
 * - HANDS_STATE: Green glow on 3 specific sensors
 * - PILLAR_STATE: Fire effects on all active sensors
 * - SOLVED_STATE: Continuous tall fire animation
 ******************************************************************************/

#include <TeensyID.h>
#include <Arduino.h>
#include <ParagonMQTT.h>

#include <math.h>
#include <OctoWS2811.h>

typedef struct { uint8_t r; uint8_t g; uint8_t b; } rgb_t;

// Literals
// Set the pillar to program here (1, 2, 3, or 4)
#ifndef PILLAR
#define PILLAR                    2
#endif

#if PILLAR == 1
const char* deviceID = "PillarOne";
#elif PILLAR == 2
const char* deviceID = "PillarTwo";
#elif PILLAR == 3
const char* deviceID = "PillarThree";
#elif PILLAR == 4
const char* deviceID = "PillarFour";
#endif
// Fire sim
#define NUMBER_OF_PANELS          3

#define LEDS_PER_PANEL            18   // 18 LEDs per section
#define BOTTOM_SECTION_START      2    // Bottom section: LEDs 2..19
#define MIDDLE_SECTION_START      26   // Middle section: LEDs 26..43
#define TOP_SECTION_START         52   // Top section: LEDs 52..69

// State 3 section fire tuning
#define SECTION_COOLING           2    // How fast a section cools (0=hottest, 9=coolest)
#define SECTION_SPARKING          100  // Probability (0-255) of adding heat when sensor active
// State 4 full-strip fire tuning
#define SOLVED_COOLING            1    // Cooler = taller flames
#define SOLVED_SPARKING           120  // Base sparking probability

// Glow fade in/out
#define GLOW_FADE_IN_RATE         2
#define GLOW_FADE_OUT_RATE        5

// LEDs
#define LEDS_PER_SIDE             144  // Each side has 144 LEDs (2 strips × 72 LEDs, mirrored/synced)
#define LEDS_IN_BOWL              40
#define NUMBER_OF_SIDES           4
#define COLOR_ORDER               GRB
#define CHIPSET                   WS2811
#define FRAMES_PER_SECOND         60
#define BRIGHTNESS                255

// Pin assignments vary by pillar - configured below based on PILLAR define
#if PILLAR == 1
  #define NUM_STRIPS              9    // 2 strips per side × 4 sides + bowl
  const int numPins = 9;
  byte pinList[numPins] = {5,6,41,40,18,17,7,11,12}; // LED pins: A1,A2,B1,B2,C1,C2,D1,D2,Bowl
  const int ledsPerStrip = 72;  // Each physical strip has 72 LEDs
  #define LED_BOWL_PIN            12   // Bowl/Top of pillar LED strip
#elif PILLAR == 2
  #define NUM_STRIPS              9    // 2 strips per side × 4 sides + bowl
  const int numPins = 9;
  byte pinList[numPins] = {17,21,6,5,11,10,40,16,12}; // LED pins: A1,A2,B1,B2,C1,C2,D1,D2,Bowl
  const int ledsPerStrip = 72;  // Each physical strip has 72 LEDs
  #define LED_BOWL_PIN            12   // Bowl/Top of pillar LED strip
#elif PILLAR == 3
  #define NUM_STRIPS              9    // 2 strips per side × 4 sides + bowl
  const int numPins = 9;
  byte pinList[numPins] = {6,5,40,41,18,21,10,7,12}; // LED pins: A1,A2,B1,B2,C1,C2,D1,D2,Bowl
  const int ledsPerStrip = 72;  // Each physical strip has 72 LEDs
  #define LED_BOWL_PIN            12   // Bowl/Top of pillar LED strip
#elif PILLAR == 4
  #define NUM_STRIPS              9    // 2 strips per side × 4 sides + bowl
  const int numPins = 9;
  byte pinList[numPins] = {5,6,10,11,18,17,40,41,12}; // LED pins: A1,A2,B1,B2,C1,C2,D1,D2,Bowl
  const int ledsPerStrip = 72;  // Each physical strip has 72 LEDs
  #define LED_BOWL_PIN            12   // Bowl/Top of pillar LED strip
#endif
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

uint16_t rand16seed;

// Pin assignments

// Raspberry Pi Serial Connection: Pins 28 (RX7), 29 (TX7) - Serial7
// Used for sending audio playback commands to Raspberry Pi

// LED Strip Pins (2 strips per side, 72 LEDs each = 144 total per side, mirrored)
// Naming: LED_[Side][Number]_PIN where Side=A/B/C/D, Number=1/2
// Bowl/Top LED is defined per-pillar in the conditional compilation section above
// Pin assignments are pillar-specific - see conditional compilation above

//Sensor Pins - Vary by pillar
// Naming: SENSOR_[Side][Location]_PIN where Side=A/B/C/D, Location=T/M/B (Top/Middle/Bottom)
#if PILLAR == 1
  // Pillar 1 sensor pins configured
  #define SENSOR_AT_PIN             4  // Side A, Top
  #define SENSOR_AM_PIN             3  // Side A, Middle
  #define SENSOR_AB_PIN             2  // Side A, Bottom
  #define SENSOR_BT_PIN             14 // Side B, Top
  #define SENSOR_BM_PIN             15 // Side B, Middle
  #define SENSOR_BB_PIN             16 // Side B, Bottom
  #define SENSOR_CT_PIN             19 // Side C, Top
  #define SENSOR_CM_PIN             20 // Side C, Middle
  #define SENSOR_CB_PIN             21 // Side C, Bottom
  #define SENSOR_DT_PIN             9  // Side D, Top
  #define SENSOR_DM_PIN             8  // Side D, Middle
  #define SENSOR_DB_PIN             10 // Side D, Bottom
#elif PILLAR == 2
  // Pillar 2 sensor pins configured
  #define SENSOR_AT_PIN             19 // Side A, Top
  #define SENSOR_AM_PIN             20 // Side A, Middle
  #define SENSOR_AB_PIN             18 // Side A, Bottom
  #define SENSOR_BT_PIN             4  // Side B, Top
  #define SENSOR_BM_PIN             3  // Side B, Middle
  #define SENSOR_BB_PIN             2  // Side B, Bottom
  #define SENSOR_CT_PIN             9  // Side C, Top
  #define SENSOR_CM_PIN             8  // Side C, Middle
  #define SENSOR_CB_PIN             7  // Side C, Bottom
  #define SENSOR_DT_PIN             14 // Side D, Top
  #define SENSOR_DM_PIN             15 // Side D, Middle
  #define SENSOR_DB_PIN             41 // Side D, Bottom
#elif PILLAR == 3
  // Pillar 3 sensor pins configured
  #define SENSOR_AT_PIN             4  // Side A, Top
  #define SENSOR_AM_PIN             3  // Side A, Middle
  #define SENSOR_AB_PIN             2  // Side A, Bottom
  #define SENSOR_BT_PIN             14 // Side B, Top
  #define SENSOR_BM_PIN             15 // Side B, Middle
  #define SENSOR_BB_PIN             16 // Side B, Bottom
  #define SENSOR_CT_PIN             19 // Side C, Top
  #define SENSOR_CM_PIN             20 // Side C, Middle
  #define SENSOR_CB_PIN             17 // Side C, Bottom
  #define SENSOR_DT_PIN             9  // Side D, Top
  #define SENSOR_DM_PIN             8  // Side D, Middle
  #define SENSOR_DB_PIN             11 // Side D, Bottom
#elif PILLAR == 4
  // Pillar 4 sensor pins configured
  #define SENSOR_AT_PIN             4  // Side A, Top
  #define SENSOR_AM_PIN             3  // Side A, Middle
  #define SENSOR_AB_PIN             2  // Side A, Bottom
  #define SENSOR_BT_PIN             9  // Side B, Top
  #define SENSOR_BM_PIN             8  // Side B, Middle
  #define SENSOR_BB_PIN             7  // Side B, Bottom
  #define SENSOR_CT_PIN             19 // Side C, Top
  #define SENSOR_CM_PIN             20 // Side C, Middle
  #define SENSOR_CB_PIN             21 // Side C, Bottom
  #define SENSOR_DT_PIN             14 // Side D, Top
  #define SENSOR_DM_PIN             15 // Side D, Middle
  #define SENSOR_DB_PIN             16 // Side D, Bottom
#endif

#define POWER_LED_PIN             13 // Onboard LED for debugging

// Sensors
#define IR_ACTIVATED              0
#define IR_NOT_ACTIVATED          1

typedef uint8_t puzzleState_t;
enum
{
  WAITING_STATE,
  HANDS_STATE,
  PILLAR_STATE,
  SOLVED_STATE,
};

enum
{
  IGNITE_BOTTOM_A_SOUND_INDEX,
  IGNITE_BOTTOM_B_SOUND_INDEX,
  IGNITE_BOTTOM_C_SOUND_INDEX,
  IGNITE_BOTTOM_D_SOUND_INDEX,
  IGNITE_MIDDLE_A_SOUND_INDEX,
  IGNITE_MIDDLE_B_SOUND_INDEX,
  IGNITE_MIDDLE_C_SOUND_INDEX,
  IGNITE_MIDDLE_D_SOUND_INDEX,
  IGNITE_TOP_A_SOUND_INDEX,
  IGNITE_TOP_B_SOUND_INDEX,
  IGNITE_TOP_C_SOUND_INDEX,
  IGNITE_TOP_D_SOUND_INDEX,
  IGNITE_BOWL_SOUND_INDEX,
  NUMBER_OF_IGNITE_SOUNDS,
  BURNING_SOUND_INDEX = NUMBER_OF_IGNITE_SOUNDS,
  NUMBER_OF_SOUNDS,
};

#define MIN_SOUND_MILLIS 500UL

// The order of this array must correspond to the above sound index enum
const char *sound_names[NUMBER_OF_SOUNDS] =
{
  "BOTTOM  WAV",
  "BOTTOM  WAV",
  "BOTTOM  WAV",
  "BOTTOM  WAV",
  "MIDDLE  WAV",
  "MIDDLE  WAV",
  "MIDDLE  WAV",
  "MIDDLE  WAV",
  "TOP     WAV",
  "TOP     WAV",
  "TOP     WAV",
  "TOP     WAV",
  "BOWL    WAV",
  "BURNING WAV"};

const char *soundKeywordFromIndex(uint8_t index)
{
  if(index <= IGNITE_BOTTOM_D_SOUND_INDEX)
  {
    return "bottom";
  }
  if(index <= IGNITE_MIDDLE_D_SOUND_INDEX)
  {
    return "middle";
  }
  if(index <= IGNITE_TOP_D_SOUND_INDEX)
  {
    return "top";
  }
  if(index == IGNITE_BOWL_SOUND_INDEX)
  {
    return "bowl";
  }
  if(index == BURNING_SOUND_INDEX)
  {
    return "burning";
  }
  return "bottom";
}

void sendSoundKeyword(const char *keyword)
{
  Serial7.println(keyword);
}

void actionState1(const char *value)
{
  (void)value;
  onState1();
}

void actionState2(const char *value)
{
  (void)value;
  onState2();
}

void actionState3(const char *value)
{
  (void)value;
  onState3();
}

void actionState4(const char *value)
{
  (void)value;
  onState4();
}

// Pillars layout in room with sides shown:
// _____________
// |  3    4   |
// |           |
// |     C     |
// |  2       B 1 D |
// |               A    |
// ---------------------
//
// Handprints (pillar-side-location): 1BB, 1DB, 2BB, 3AT
// Pillar solutions (pillar-side-location): 
//   Pillar 1: DT, CM, CT, BT
//   Pillar 2: AT, AM, BM, CT
//   Pillar 3: DT, DB, CM, BB
//   Pillar 4: BM, BB, AM, DB
// Location codes: T=Top, M=Middle, B=Bottom

// Map from sensor pins to positions
enum
{
  BOTTOM_POSITION_INDEX,
  MIDDLE_POSITION_INDEX,
  TOP_POSITION_INDEX,
};

const struct 
{
  boolean handprint_solution;    
  boolean pillar_solution;
  uint8_t pin;
  uint16_t led_section_start;
  uint8_t ignite_sound_index;
  uint8_t side_index;
  uint8_t position_index;  
} sensor_info[] =
{
  #if PILLAR == 1
  { false, false, SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },
  { false, false, SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, true,  SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },
  { true,  false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
  #elif PILLAR == 2
  { false, true,  SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },
  { false, true,  SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },
  { true,  false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, false, SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
  #elif PILLAR == 3
  { true,  false, SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },
  { false, false, SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },
  { false, true,  SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, true,  SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },
  { false, true,  SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
  #elif PILLAR == 4
  { false, false, SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },
  { false, true,  SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, true,  SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },
  { false, true,  SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, false, SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },
  { false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },
  { false, true,  SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
  #endif
};
#define NUMBER_OF_SENSORS (sizeof(sensor_info) / sizeof(sensor_info[0]))

// Globals
char soundCommand[32] = {0};  // Buffer for sound commands to Raspberry Pi

static uint32_t last_sound_millis;

static boolean sensors[NUMBER_OF_SENSORS];
static uint8_t heat[NUMBER_OF_SIDES][72];   // One heat cell per LED (0-255 scale)
static uint8_t bowl_heat[LEDS_IN_BOWL];
static uint8_t glow[NUMBER_OF_SIDES][NUMBER_OF_PANELS];
static puzzleState_t puzzle_state;
static boolean last_ignite_sounds[NUMBER_OF_IGNITE_SOUNDS];

// Functions

// This generates a normal (Bell curve) distribution given two independent random
// numbers in [0, 1]
inline float boxMuller(float u1, float u2)
{
  // ---- replaced by step_fire() below ----
}

void setup() 
{
pinMode(POWER_LED_PIN, OUTPUT);
digitalWrite(POWER_LED_PIN, HIGH);
uint8_t side;
uint16_t i;

int state = 1;

// Set up Raspberry Pi serial
Serial7.begin(115200);  // Serial7 (pins 28/29) for Raspberry Pi audio commands

puzzle_state = WAITING_STATE;


leds.begin();
leds.show();

// Set up sensor inputs
for(i = 0; i < NUMBER_OF_SENSORS; i++)
  {
  pinMode(sensor_info[i].pin, INPUT_PULLUP);
  }


// Initialize fire sim
for(side = 0; side < NUMBER_OF_SIDES; side++)
  {
  memset(heat[side], 0, sizeof(heat[side]));
  }
  
  networkSetup();
  mqttSetup();
  registerAction("state1", actionState1);
  registerAction("state2", actionState2);
  registerAction("state3", actionState3);
  registerAction("state4", actionState4);
}

// Updates the state of the puzzle as a function of input from sensors and other puzzles
void updatePuzzleState(puzzleState_t *state, uint32_t current_millis)
{
#define DELAY_BEFORE_PILLAR_ENABLED 5000UL
#define INPUT_DEBOUNCE_TIME         100UL

boolean no_solution;
boolean valid_solution;
boolean all_sensors;


//boolean pillar_override;
uint16_t i;

static uint32_t handprints_all_solved_millis;
  
switch(*state)
  {
  default:
    break;
}
}

rgb_t calculate_heat_color(float heat_val)
{
  // ---- replaced by heatColor() / applyPillarColor() below ----
}

// Classic cellular-automaton fire step over heat[start .. start+len-1].
// inject_spark=true drives continuous fire from the base (used for State 4 and bowl).
// In State 3 the caller injects sparks manually when a sensor trips.
void step_fire(uint8_t *heat, int start, int len, bool inject_spark)
{
  // 1. Cool every cell a little
  for(int i = start; i < start + len; i++)
  {
    uint8_t cool = random(0, SECTION_COOLING + 2);
    heat[i] = (heat[i] > cool) ? heat[i] - cool : 0;
  }
  // 2. Drift heat upward (higher index = physically higher on strip)
  for(int k = start + len - 1; k >= start + 2; k--)
  {
    heat[k] = ((uint16_t)heat[k-1] + heat[k-2] + heat[k-2]) / 3;
  }
  // 3. Optionally auto-spark at the base
  if(inject_spark && random(255) < SOLVED_SPARKING)
  {
    int y = start + random(min(5, len));
    heat[y] = qadd8(heat[y], random(160, 255));
  }
}

// Map a 0-255 heat value to an RGB colour (black -> red -> orange -> yellow -> white).
// This is the classic FastLED-style trilinear fire palette.
rgb_t heatColor(uint8_t temperature)
{
  rgb_t c;
  uint8_t t192 = (uint32_t)temperature * 191 / 255;
  uint8_t ramp = (t192 & 0x3F) << 2;   // 0-252 ramp within each third
  if(t192 & 0x80)       { c.r = 255; c.g = 255; c.b = ramp; }  // hottest: yellow->white
  else if(t192 & 0x40)  { c.r = 255; c.g = ramp; c.b = 0;   }  // middle:  orange->yellow
  else                  { c.r = ramp; c.g = 0;   c.b = 0;   }  // coolest: black->red
  return c;
}

// Apply the per-pillar colour tint in PILLAR_STATE (State 3).
// In all other states natural fire colours are used unchanged.
rgb_t applyPillarColor(rgb_t c)
{
  if(puzzle_state != PILLAR_STATE) return c;
  rgb_t out;
  // Use a single brightness scalar to force a strict pillar hue in State 3.
  // This prevents hot pixels from drifting toward white.
  uint8_t intensity = max(c.r, max(c.g, c.b));
  #if PILLAR == 1   // Red
    out.r = intensity; out.g = 0;         out.b = 0;
  #elif PILLAR == 2 // Blue
    out.r = 0;         out.g = 0;         out.b = intensity;
  #elif PILLAR == 3 // Green
    out.r = 0;         out.g = intensity; out.b = 0;
  #elif PILLAR == 4 // Yellow
    out.r = intensity; out.g = intensity; out.b = 0;
  #endif
  return out;
}

void setColor(int side, int ledIndex, rgb_t color){
  // Each side has 144 LEDs total: 2 strips of 72 LEDs each that mirror/sync
  // Strip layout in pinList: A1,A2,B1,B2,C1,C2,D1,D2
  // When setting a color, write to BOTH strips for that side
  // side 0 (A) = strips 0,1 | side 1 (B) = strips 2,3 | etc.
  
  int stripPair = side * 2;  // First strip index of the pair for this side
  
  // Write to both strips simultaneously (mirrored)
  leds.setPixel((stripPair * ledsPerStrip) + ledIndex, color.r, color.g, color.b);           // Strip 1
  leds.setPixel(((stripPair + 1) * ledsPerStrip) + ledIndex, color.r, color.g, color.b);     // Strip 2
}

void setColor_bowl(int ledIndex, rgb_t color) {
  // Bowl is on strip 8 (9th strip in pinList)
  // Strip 8 starts at pixel index: 8 * ledsPerStrip = 8 * 72 = 576
  leds.setPixel((8 * ledsPerStrip) + ledIndex, color.r, color.g, color.b);
}

void blackoutAllLeds()
{
  for(int pixel = 0; pixel < ledsPerStrip * numPins; pixel++)
  {
    leds.setPixel(pixel, 0, 0, 0);
  }
  leds.show();
}

void clearAllSensorsToInactive()
{
  for(uint16_t idx = 0; idx < NUMBER_OF_SENSORS; idx++)
  {
    sensors[idx] = IR_NOT_ACTIVATED;
  }
}

uint8_t qadd8(uint8_t i, uint8_t j){
  int16_t x;
  x = (int16_t)i + (int16_t)j;
  if (x > 255) x = 255;
  return ((uint8_t)x);
}

uint8_t qsub8(uint8_t i, uint8_t j){
  int16_t x;
  x = (int16_t)i - (int16_t)j;
  if (x < 0) x = 0;
  return ((uint8_t)x);
}



void loop() {
uint32_t current_millis = millis();

if(puzzle_state != WAITING_STATE){
uint16_t brightness;  // used in HANDS_STATE glow rendering
uint16_t i;
uint16_t j;
uint8_t side;
rgb_t color;

// Sound
boolean is_something_burning;
boolean is_sound_playing;
boolean ignite_sounds[NUMBER_OF_IGNITE_SOUNDS];
puzzleState_t prev_puzzle_state;

// Read the sensors
if(puzzle_state == SOLVED_STATE)
{
  // State 4 ignores all sensor input and runs final animation continuously.
  clearAllSensorsToInactive();
}
else
{
  for(i = 0; i < NUMBER_OF_SENSORS; i++)
  {
    sensors[i] = digitalRead(sensor_info[i].pin);
  }
}


  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",sensors[0],sensors[1],sensors[2],sensors[3],sensors[4],sensors[5],sensors[6],sensors[7],sensors[8],sensors[9],sensors[10],sensors[11]);
  
// Check if minimum time has elapsed since last sound started
is_sound_playing = (last_sound_millis + MIN_SOUND_MILLIS > current_millis);


// Process logic for each side of the pillar as a function of sensor input and puzzle state
memset(ignite_sounds, 0, sizeof(ignite_sounds));

  // Trigger lighting and sound effects for each pillar side
  for(side = 0; side < NUMBER_OF_SIDES; side++)
    {
    if(puzzle_state == SOLVED_STATE)
      {
      // ---------------------------------------------------------------
      // STATE 4: Full-strip fire across all 72 LEDs — no section logic.
      // step_fire() auto-injects sparks at the base each frame.
      // ---------------------------------------------------------------
      step_fire(heat[side], 0, ledsPerStrip, true);
      for(j = 0; j < ledsPerStrip; j++)
        {
        color = heatColor(heat[side][j]);
        setColor(side, j, color);
        }
      ignite_sounds[IGNITE_BOTTOM_A_SOUND_INDEX] = true;
      }
    else if(puzzle_state == PILLAR_STATE)
      {
      // ---------------------------------------------------------------
      // STATE 3: Per-section fire. Each section (18 LEDs) is a separate
      // flame driven by its sensor. Sections don't bleed into each other.
      // ---------------------------------------------------------------
      const uint16_t section_starts[NUMBER_OF_PANELS] = {
        BOTTOM_SECTION_START, MIDDLE_SECTION_START, TOP_SECTION_START
      };
      for(uint8_t pos = 0; pos < NUMBER_OF_PANELS; pos++)
        {
        uint16_t sec = section_starts[pos];
        // Check every sensor on this side that belongs to this section
        for(i = 0; i < NUMBER_OF_SENSORS; i++)
          {
          if(sensor_info[i].side_index != side) continue;
          if(sensor_info[i].led_section_start != sec) continue;
          if(sensors[i] == IR_ACTIVATED)
            {
            // Sensor is active — inject a burst of heat at the section base
            heat[side][sec] = qadd8(heat[side][sec], random(130, 220));
            ignite_sounds[sensor_info[i].ignite_sound_index] = true;
            }
          }
        // Advance the fire simulation for this section only
        step_fire(heat[side], sec, LEDS_PER_PANEL, false);
        // Render section LEDs with pillar-tinted fire colour
        for(j = sec; j < sec + LEDS_PER_PANEL; j++)
          {
          color = applyPillarColor(heatColor(heat[side][j]));
          setColor(side, j, color);
          }
        } // end for each section
      }
    else if(puzzle_state == HANDS_STATE)
      {
      // ---------------------------------------------------------------
      // STATE 2: Green glow on handprint sensor positions only.
      // ---------------------------------------------------------------
      for(i = 0; i < NUMBER_OF_SENSORS; i++)
        {
        if(sensor_info[i].side_index != side) continue;
        if(sensor_info[i].handprint_solution)
          {
          if(sensors[i] == IR_ACTIVATED)
            glow[side][sensor_info[i].position_index] = qadd8(glow[side][sensor_info[i].position_index], GLOW_FADE_IN_RATE);
          else
            glow[side][sensor_info[i].position_index] = qsub8(glow[side][sensor_info[i].position_index], GLOW_FADE_OUT_RATE);
          }
        for(j = sensor_info[i].led_section_start; j < sensor_info[i].led_section_start + LEDS_PER_PANEL; j++)
          {
          color.r = 0;
          color.g = (uint8_t)glow[side][sensor_info[i].position_index];
          color.b = 0;
          setColor(side, j, color);
          }
        }
      }
    } // end for each side
  
  // Bowl: active in State 3 and State 4, black otherwise.
  if(puzzle_state == PILLAR_STATE || puzzle_state == SOLVED_STATE)
    {
    step_fire(bowl_heat, 0, LEDS_IN_BOWL, true);
    ignite_sounds[IGNITE_BOWL_SOUND_INDEX] = true;
    for(int a = 0; a < LEDS_IN_BOWL; a++)
      {
      color = applyPillarColor(heatColor(bowl_heat[a]));
      setColor_bowl(a, color);
      }
    }
  else
    {
    color.r = 0; color.g = 0; color.b = 0;
    for(int a = 0; a < LEDS_IN_BOWL; a++) setColor_bowl(a, color);
    }

// Process sound effects if in Pillar Enabled State
if(puzzle_state == PILLAR_STATE)
  {
  is_something_burning = false;
  for(i = 0; i < NUMBER_OF_IGNITE_SOUNDS; i++)
    {
    is_something_burning |= ignite_sounds[i];

    // A new burn was detected at a different position, interrupt and play the new burn sound.
    if(!last_ignite_sounds[i] && ignite_sounds[i])
      {
      if(is_sound_playing)
        {
        sendSoundKeyword("stop");
        last_sound_millis = current_millis - MIN_SOUND_MILLIS;
        }
      sendSoundKeyword(soundKeywordFromIndex(i));
      last_sound_millis = current_millis;
      is_sound_playing = true;
      break;
      }
    }
  }


// Process sound effects (except easter egg or Pillar Enabled)
if(puzzle_state != PILLAR_STATE)
  {
  is_something_burning = false;
  for(i = 0; i < NUMBER_OF_IGNITE_SOUNDS; i++)
    {
    is_something_burning |= ignite_sounds[i];

    // A new burn was detected at a different position, interrupt and play the new burn sound.
    if(!last_ignite_sounds[i] && ignite_sounds[i])
      {
      if(is_sound_playing)
        {
        sendSoundKeyword("stop");
        last_sound_millis = current_millis - MIN_SOUND_MILLIS;
        }
      sendSoundKeyword(soundKeywordFromIndex(i));
      last_sound_millis = current_millis;
      is_sound_playing = true;
      break;
      }
    }

  // Play the background burn sound if the initial sound has finished playing
  // but something is still burning.        
  if(is_something_burning && !is_sound_playing)
    {
    sendSoundKeyword("burning");
    last_sound_millis = current_millis;
    }
  else if(!is_something_burning && is_sound_playing)
    {
    sendSoundKeyword("stop");
    last_sound_millis = current_millis - MIN_SOUND_MILLIS;
    }
  }
memcpy(last_ignite_sounds, ignite_sounds, sizeof(last_ignite_sounds));

leds.show();
} else {
  snprintf(publishDetail, sizeof(publishDetail), "Waiting for Jackal and Ankh");
}

// Always service MQTT regardless of state
sendDataMQTT();
}

void onState1(){
  puzzle_state = WAITING_STATE;
  clearAllSensorsToInactive();
  blackoutAllLeds();
}

void onState2(){
  puzzle_state = HANDS_STATE;
  clearAllSensorsToInactive();
  blackoutAllLeds();
  mqttBroker();
  publish("CONNECTED");
}

void onState3(){
  puzzle_state = PILLAR_STATE;
  clearAllSensorsToInactive();
  blackoutAllLeds();
}

void onState4(){
  puzzle_state = SOLVED_STATE;
  clearAllSensorsToInactive();
  blackoutAllLeds();
}