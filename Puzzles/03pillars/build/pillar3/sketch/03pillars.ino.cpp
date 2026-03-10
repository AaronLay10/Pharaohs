#line 1 "/private/tmp/p3/03pillars/03pillars.ino"
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
#include <NativeEthernet.h>
#include <PubSubClient.h>

uint8_t mac[6];

#include <math.h>
#include <OctoWS2811.h>
#include "functions.h"
#include "color_temp_lookup.h"
#include "rgb_888.h"

// Literals
// Set the pillar to program here (1, 2, 3, or 4)
#define PILLAR                    3
// Fire sim - 5.5" between sections, 35" from top section to bowl
#define NUMBER_OF_HEAT_CELLS      160  // Scaled for 144 LEDs per side
#define NUMBER_OF_BOWL_HEAT_CELLS 40
#define NUMBER_OF_PANELS          3
#define NUMBER_OF_BURNS           4

#define LEDS_PER_PANEL            18   // 18 LEDs per section in 72-pixel mirrored strip space
#define BOTTOM_SECTION_START      2    // Bottom section starts near strip base
#define MIDDLE_SECTION_START      26   // Middle section starts above bottom
#define TOP_SECTION_START         52   // Top section has the highest pixel indices
#define BOWL_HEIGHT               2
#define BURN_OFFSET               3    // Add this to make fire sim look better when burning at the bottom

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


// These are the max values for each RGB channel, used for white balance
#define MAX_RED                   255
#define MAX_GREEN                 224
#define MAX_BLUE                  140

// LED color temperature
#define BRIGHTNESS_SCALE          8000
#define DISPLAY_BLACKOUT_TEMP     1100
#define DISPLAY_MIN_COLOR_TEMP    1024
#define DISPLAY_MAX_COLOR_TEMP    20000

// Sensors
#define IR_ACTIVATED              0
#define IR_NOT_ACTIVATED          1

// Fire sim inputs
#define INITIAL_HEAT              300

// mqtt message variables
#define MQTTDELAY 200
long previousMillisMqtt = 0;
boolean solveOne = false;
boolean solveTwo = false;
boolean overrideOne = false;
boolean overrideTwo = false;
boolean resetOne = false;
boolean resetTwo = false;
boolean startPillars = false;

typedef struct 
{
  float diffusion;
  float velocity;
  float cooling;
  float ambient;
  float mean;
  float stddev;
} fire_params_t;

const fire_params_t bowl_fire   = { 2.7e-4f, 0.2f, 40.0f,  300.0f, 1500.0f, 200.0f };
const fire_params_t normal_fire = { 2.7e-4f, 0.2f, 400.0f, 300.0f, 1800.0f, 150.0f };
const fire_params_t tall_fire   = { 1.7e-4f, 0.9f, 130.0f, 300.0f, 2500.0f, 220.0f };

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
  IGNITE_ALL_SOUND_INDEX,
  NUMBER_OF_IGNITE_SOUNDS,
  BURNING_SOUND_INDEX = NUMBER_OF_IGNITE_SOUNDS,
  EASTER_EGG_SOUND_INDEX,
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
  "LOUD    WAV",
  "BURNING WAV"};

#line 273 "/private/tmp/p3/03pillars/03pillars.ino"
const char * soundKeywordFromIndex(uint8_t index);
#line 302 "/private/tmp/p3/03pillars/03pillars.ino"
void sendSoundKeyword(const char *keyword);
#line 434 "/private/tmp/p3/03pillars/03pillars.ino"
float boxMuller(float u1, float u2);
#line 440 "/private/tmp/p3/03pillars/03pillars.ino"
float randnorm(float mean, float stddev);
#line 446 "/private/tmp/p3/03pillars/03pillars.ino"
float derivative1(float *vals, int n, int x);
#line 457 "/private/tmp/p3/03pillars/03pillars.ino"
float derivative2(float *vals, int n, int x);
#line 469 "/private/tmp/p3/03pillars/03pillars.ino"
float calc_heat_diffusion(float *heat, uint16_t n, float k, uint16_t x);
#line 477 "/private/tmp/p3/03pillars/03pillars.ino"
float calc_velocity_temp_directional(float *heat, uint16_t n, float vx, uint16_t x);
#line 485 "/private/tmp/p3/03pillars/03pillars.ino"
void simulate_fire(float *heat, uint16_t number_of_heat_cells, float *burn, uint16_t *section_starts, uint16_t number_of_burns, fire_params_t params);
#line 540 "/private/tmp/p3/03pillars/03pillars.ino"
void setup();
#line 587 "/private/tmp/p3/03pillars/03pillars.ino"
void updatePuzzleState(puzzleState_t *state, uint32_t current_millis);
#line 609 "/private/tmp/p3/03pillars/03pillars.ino"
rgb_t calculate_heat_color(float heat_val);
#line 669 "/private/tmp/p3/03pillars/03pillars.ino"
void setColor(int side, int ledIndex, rgb_t color);
#line 682 "/private/tmp/p3/03pillars/03pillars.ino"
void setColor_bowl(int ledIndex, rgb_t color);
#line 688 "/private/tmp/p3/03pillars/03pillars.ino"
uint8_t qadd8(uint8_t i, uint8_t j);
#line 695 "/private/tmp/p3/03pillars/03pillars.ino"
uint8_t qsub8(uint8_t i, uint8_t j);
#line 704 "/private/tmp/p3/03pillars/03pillars.ino"
void loop();
#line 935 "/private/tmp/p3/03pillars/03pillars.ino"
void onState1();
#line 940 "/private/tmp/p3/03pillars/03pillars.ino"
void onState2();
#line 946 "/private/tmp/p3/03pillars/03pillars.ino"
void onState3();
#line 951 "/private/tmp/p3/03pillars/03pillars.ino"
void onState4();
#line 31 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
void mqttCallback(char* topic, byte* payload, unsigned int length);
#line 61 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
void ethernetSetup();
#line 80 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
void mqttSetup();
#line 87 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
void mqttLoop();
#line 131 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
void publish(char* message);
#line 273 "/private/tmp/p3/03pillars/03pillars.ino"
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
  if(index == IGNITE_ALL_SOUND_INDEX)
  {
    return "loud";
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

// Pillars layout in room with sides shown:
// _____________
// |  3    4   |
// |           |
// |     C     |
// |  2       B 1 D |
// |               A    |
// ---------------------
//
// Handprints (pillar-side-location): 1BB, 2BB, 3AT
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
  { true,  false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },
  
  { false, true,  SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, true,  SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },  
  { false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, true,  SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },  
  { false, false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
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
char publishDetail[50] = {0};
char soundCommand[32] = {0};  // Buffer for sound commands to Raspberry Pi

extern const rgb_888_t color_temp_to_rgb[];



static uint32_t last_sound_millis;

static boolean sensors[NUMBER_OF_SENSORS];
//static int leds[NUMBER_OF_SIDES][LEDS_PER_SIDE];
//static int bowl_leds[LEDS_IN_BOWL];
static float heat[NUMBER_OF_SIDES][NUMBER_OF_HEAT_CELLS];
static float bowl_heat[NUMBER_OF_BOWL_HEAT_CELLS];
static uint8_t glow[NUMBER_OF_SIDES][NUMBER_OF_PANELS];
static puzzleState_t puzzle_state;
static boolean last_ignite_sounds[NUMBER_OF_IGNITE_SOUNDS];

// Functions

// This generates a normal (Bell curve) distribution given two independent random
// numbers in [0, 1]
inline float boxMuller(float u1, float u2)
{
  return(sqrt(-2*log(u1))*cos(2*M_PI*u2));
}

// Generate a random variable with specified mean and standard deviation
inline float randnorm(float mean, float stddev)
{
  return(mean + stddev * boxMuller(random(65535) / 65535.0, random(65535) / 65535.0));
}

// Calculate the 1st derivative at x
inline float derivative1(float *vals, int n, int x)
{
  int i1;
  int i3;
  
  i1 = max(min(x - 1, n - 1), 0);
  i3 = max(min(x + 1, n - 1), 0);
  return((vals[i3] - vals[i1]) / 2.0);
}

// Calculate the 2nd derivative at x
inline float derivative2(float *vals, int n, int x)
{
  int i1;
  int i2;
  int i3;

  i1 = max(min(x - 1, n - 1), 0);
  i2 = x;
  i3 = max(min(x + 1, n - 1), 0);
  return((vals[i3] - vals[i2]) - (vals[i2] - vals[i1]));
}

float calc_heat_diffusion(float *heat, uint16_t n, float k, uint16_t x)
{
  float d2x;
  
  d2x = derivative2(heat, n, x);
  return(k * d2x);
}

float calc_velocity_temp_directional(float *heat, uint16_t n, float vx, uint16_t x)
{
  float dx;
    
  dx = derivative1(heat, n, x);
  return(vx * dx);
}

void simulate_fire(float *heat, 
                   uint16_t number_of_heat_cells, 
                   float *burn, 
                   uint16_t *section_starts, 
                   uint16_t number_of_burns,
                   fire_params_t params)
{ 
float cooling[NUMBER_OF_HEAT_CELLS];
float heat_directional[NUMBER_OF_HEAT_CELLS];
float heat_diffusion[NUMBER_OF_HEAT_CELLS];
uint16_t i;
float max_heat;
  
// Fire simulation: model LED temperature and brightness
// Based on http://graphics.berkeley.edu/papers/Feldman-ASP-2003-08/Feldman-ASP-2003-08.pdf
// NOTE: Cell 0 is reserved as a boundary condition
heat_directional[0] = 0;
heat_diffusion[0] = 0;
cooling[0] = 0;
max_heat = 0;
for(i = 1; i < number_of_heat_cells; i++)
  {
  heat_directional[i] = calc_velocity_temp_directional(heat, number_of_heat_cells, params.velocity, i); 
  heat_diffusion[i] = calc_heat_diffusion(heat, number_of_heat_cells, params.diffusion, i); 
  max_heat = max(heat[i], max_heat);
  }

for(i = 1; i < number_of_heat_cells; i++)
  {
  float cooling_ratio;
  
  if( max_heat != params.ambient )
    {
    cooling_ratio = (heat[i] - params.ambient) / (max_heat - params.ambient);
    cooling[i] = params.cooling * cooling_ratio * cooling_ratio * cooling_ratio * cooling_ratio;
    }
  else
    {
    cooling[i] = 0;
    }
  }

for(i = 0; i < number_of_heat_cells; i++)
  {
  heat[i] += -cooling[i] - heat_directional[i] + heat_diffusion[i] * heat[i];
  heat[i] = max(heat[i], params.ambient);
  heat[i] = min(heat[i], DISPLAY_MAX_COLOR_TEMP);
  }

for(i = 0; i < number_of_burns; i++)
  {
  heat[section_starts[i]] += burn[i];
  }
}

void setup() 
{
pinMode(POWER_LED_PIN, OUTPUT);
digitalWrite(POWER_LED_PIN, HIGH);
uint8_t side;
uint16_t i;

int state = 1;

// Set up debug serial and Raspberry Pi serial
Serial.begin(115200);
Serial7.begin(115200);  // Serial7 (pins 28/29) for Raspberry Pi audio commands

teensyMAC(mac);
Serial.printf("String MAC Address: %s\n", teensyMAC());

puzzle_state = WAITING_STATE;


leds.begin();
leds.show();

// Set up general I/O



// Set up sensor inputs
for(i = 0; i < NUMBER_OF_SENSORS; i++)
  {
  pinMode(sensor_info[i].pin, INPUT_PULLUP);
  }


// Initialize fire sim
for(side = 0; side < NUMBER_OF_SIDES; side++)
  {
  for(i = 0; i < NUMBER_OF_HEAT_CELLS; i++)
    {
    heat[side][i] = INITIAL_HEAT;
    }
  }
  
  ethernetSetup();
  mqttSetup();
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
uint16_t brightness;
rgb_888_t rgb;
rgb_t color;

memcpy_P(&rgb, 
         &color_temp_to_rgb[KELVIN_TO_IDX(min(max((int)heat_val, DISPLAY_MIN_COLOR_TEMP), DISPLAY_MAX_COLOR_TEMP))],
         sizeof(rgb));

if(heat_val < DISPLAY_BLACKOUT_TEMP)
  {
  brightness = 0;
  }
else
  {
  // Use Stefan-Boltmann proportion for brightness as a function of temperature
  brightness =
    (uint16_t)min(heat_val * heat_val * heat_val * heat_val * 
                  (float)BRIGHTNESS_SCALE / 
                 ((float)DISPLAY_MIN_COLOR_TEMP * 
                  (float)DISPLAY_MIN_COLOR_TEMP * 
                  (float)DISPLAY_MIN_COLOR_TEMP * 
                  (float)DISPLAY_MIN_COLOR_TEMP),
                  65535);
  }  

rgb.r = ((uint32_t)rgb.r * (uint32_t)brightness) >> 16;
rgb.g = ((uint32_t)rgb.g * (uint32_t)brightness) >> 16;
rgb.b = ((uint32_t)rgb.b * (uint32_t)brightness) >> 16;

if(PILLAR == 1 && puzzle_state == PILLAR_STATE){
  color.r = rgb.r;
  color.g = 0;
  color.b = 0;
}
else if(PILLAR == 2 && puzzle_state == PILLAR_STATE){
  color.r = rgb.b;
  color.g = rgb.g;
  color.b = rgb.r;
}
else if(PILLAR == 3 && puzzle_state == PILLAR_STATE){
  color.r = rgb.g;
  color.g = rgb.r;
  color.b = rgb.b;
}
else if(PILLAR == 4 && puzzle_state == PILLAR_STATE){
  color.r = rgb.r; //rgb.r
  color.g = rgb.r; //rgb.r
  color.b = rgb.b; //rgb.b
}
else {
  color.r = rgb.r;
  color.g = rgb.g;
  color.b = rgb.b;
}

return(color);
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
if(startPillars && puzzle_state != WAITING_STATE){
float burn[NUMBER_OF_PANELS];
float bowl_burn;
uint16_t bowl_burn_height = BOWL_HEIGHT;
uint16_t burn_heights[NUMBER_OF_PANELS];
uint8_t burn_index;
float heat_val;
fire_params_t fire_params = normal_fire;
uint16_t brightness;
uint16_t i;
uint16_t j;
uint8_t side;
rgb_t color;

// Sound
boolean is_something_burning;
boolean is_sound_playing;
boolean ignite_sounds[NUMBER_OF_IGNITE_SOUNDS];

uint32_t current_millis = millis();
puzzleState_t prev_puzzle_state;

// Read the sensors
for( i = 0; i < NUMBER_OF_SENSORS; i++ )
  {
  sensors[i] = digitalRead(sensor_info[i].pin);
  }


  unsigned long currentMillisMqtt = millis();
  if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",sensors[0],sensors[1],sensors[2],sensors[3],sensors[4],sensors[5],sensors[6],sensors[7],sensors[8],sensors[9],sensors[10],sensors[11]);
  publish(publishDetail);
  Serial.println(publishDetail);
  Serial.println(puzzle_state);
  mqttLoop();
  }
  
// Check if minimum time has elapsed since last sound started
is_sound_playing = (last_sound_millis + MIN_SOUND_MILLIS > current_millis);


// Process logic for each side of the pillar as a function of sensor input and puzzle state
memset(ignite_sounds, 0, sizeof(ignite_sounds));

  // Trigger lighting and sound effects for each pillar side
  for(side = 0; side < NUMBER_OF_SIDES; side++)
    {
    memset(burn, 0, sizeof(burn));
    memset(burn_heights, 0, sizeof(burn_heights));
    burn_index = 0;
    for(i = 0; i < NUMBER_OF_SENSORS; i++)
      {
      if(side != sensor_info[i].side_index)
        {
        continue;
        }
      burn_heights[burn_index] = sensor_info[i].led_section_start + BURN_OFFSET;
      switch(puzzle_state)
        {          
          case HANDS_STATE:
            if(sensor_info[i].handprint_solution)
              {
              if(sensors[i] == IR_ACTIVATED)
                {
                glow[side][sensor_info[i].position_index] = qadd8(glow[side][sensor_info[i].position_index], GLOW_FADE_IN_RATE);
                      
                }
              else
                {
                glow[side][sensor_info[i].position_index] = qsub8(glow[side][sensor_info[i].position_index], GLOW_FADE_OUT_RATE);
                               
                }
              }
            break;       
          
          case PILLAR_STATE:
            if(sensors[i] == IR_ACTIVATED)
              {
              burn[burn_index++] = randnorm(normal_fire.mean, normal_fire.stddev);
              ignite_sounds[sensor_info[i].ignite_sound_index] = true;
              }
            break;
  
          case SOLVED_STATE:
            fire_params = tall_fire;
            if(sensor_info[i].led_section_start == BOTTOM_SECTION_START)
              {
              burn[burn_index++] = randnorm(tall_fire.mean, tall_fire.stddev);
              }
            ignite_sounds[IGNITE_ALL_SOUND_INDEX] = true;
            break;
        } // end switch puzzle state
      } // end for each sensor

    // Run fire simulation
    simulate_fire(heat[side], NUMBER_OF_HEAT_CELLS, burn, burn_heights, burn_index, fire_params);

    // Convert values to RGBA and display on appropriate LEDs for each sensor
    for(i = 0; i < NUMBER_OF_SENSORS; i++)
      {
      if(side != sensor_info[i].side_index)
        {
        continue;
        }
      for(j = sensor_info[i].led_section_start; j < sensor_info[i].led_section_start + LEDS_PER_PANEL; j++)
        {
        if(puzzle_state == HANDS_STATE){
          brightness = (uint16_t)glow[side][sensor_info[i].position_index];
          color.r = 0;
          color.g = brightness;
          color.b = 0;
          }
          else
            {
            heat_val = (int)heat[side][j + BURN_OFFSET];
            color = calculate_heat_color(heat_val);
            }
          
          setColor(side,j, color);
          }      
        } // end for each sensor
      } // end for each side

      // Trigger bowl lighting effects
      if(puzzle_state == PILLAR_STATE 
      || puzzle_state == SOLVED_STATE)
        {
        bowl_burn = randnorm(bowl_fire.mean, bowl_fire.stddev);
        }
        else
        {
        bowl_burn = 0;
        }
   
        simulate_fire(bowl_heat, NUMBER_OF_BOWL_HEAT_CELLS, &bowl_burn, &bowl_burn_height, 1, bowl_fire);

      for(int a = 0; a < LEDS_IN_BOWL; a++)
      {
      heat_val = (int)bowl_heat[a];
      color = calculate_heat_color(heat_val);
      setColor_bowl(a, color);
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
        Serial.println("Stop sound due to different burn position");
        }
      Serial.print("Play ");
      Serial.println((char *)sound_names[i]);
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
        Serial.println("Stop sound due to different burn position");
        }
      Serial.print("Play ");
      Serial.println((char *)sound_names[i]);
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
    Serial.print("Play ");
    Serial.println((char *)sound_names[BURNING_SOUND_INDEX]);
    sendSoundKeyword("burning");
    last_sound_millis = current_millis;
    }
  else if(!is_something_burning && is_sound_playing)
    {
    sendSoundKeyword("stop");
    last_sound_millis = current_millis - MIN_SOUND_MILLIS;
    Serial.println("Stop sound since nothing is burning");
    }
  }
memcpy(last_ignite_sounds, ignite_sounds, sizeof(last_ignite_sounds));

leds.show();
} else {
    unsigned long currentMillisMqtt = millis();
  if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    publish("Waiting for Jackal and Ankh");
    Serial.println("Waiting for Jackal and Ankh");
  mqttLoop();
  }
}
}

void onState1(){
  startPillars = false;
  puzzle_state = WAITING_STATE;
}

void onState2(){
  startPillars = true;
  puzzle_state = HANDS_STATE;
  publish("CONNECTED");
}

void onState3(){
  startPillars = true;
  puzzle_state = PILLAR_STATE;
}

void onState4(){
  startPillars = true;
  puzzle_state = SOLVED_STATE;
}

#line 1 "/private/tmp/p3/03pillars/EthernetMQTT.ino"
//Arduino Ethernet MQTT

const IPAddress mqttServerIP(192, 168, 20, 8);
// Unique name of this device, used as client ID and Topic Name on MQTT
#if PILLAR == 1
  const char* deviceID = "PillarOne";
#elif PILLAR == 2
  const char* deviceID = "PillarTwo";
#elif PILLAR == 3
  const char* deviceID = "PillarThree";
#elif PILLAR == 4
  const char* deviceID = "PillarFour";
#endif

// Global Variables 
// Create an instance of the Ethernet client
EthernetClient ethernetClient;
// Create an instance of the MQTT client based on the ethernet client
PubSubClient MQTTclient(ethernetClient);
// The time (from millis()) at which last message was published
long lastMsgTime = 0;
// A buffer to hold messages to be sent/have been received
char msg[64];
// The topic in which to publish a message
char topic[32];
// Counter for number of heartbeat pulses sent
int pulseCount = 0;

// Callback function each time a message is published in any of
// the topics to which this client is subscribed
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // The message "payload" passed to this function is a byte*
  // Let's first copy its contents to the msg char[] array
  memcpy(msg, payload, length);
  // Add a NULL terminator to the message to make it a correct string
  msg[length] = '\0';

  // Debug
  Serial.print("Message received in topic [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // Act upon the message received

  if(strcmp(msg, "state1") == 0) {
    onState1();
  }
  else if(strcmp(msg, "state2") == 0) {
    onState2();
  }
  else if(strcmp(msg, "state3") == 0) {
    onState3();
  }
  else if(strcmp(msg, "state4") == 0) {
    onState4();
  }
}

void ethernetSetup() {

  if(!Serial) {
    // Start the serial connection
    Serial.begin(9600);
  }
  
  // start the Ethernet connection:

  Serial.println("Initialize Ethernet:");

  Ethernet.begin(mac);
 
  
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
}

void mqttSetup() {
  // Define some settings for the MQTT client
  MQTTclient.setServer(mqttServerIP, 33002);
  MQTTclient.setCallback(mqttCallback);
  
}

void mqttLoop() {
  // Ensure there's a connection to the MQTT server
  while (!MQTTclient.connected()) {

    // Debug info
    Serial.print("Attempting to connect to MQTT broker at ");
    Serial.println(mqttServerIP);

    // Attempt to connect
    if (MQTTclient.connect(deviceID)) {
    
      // Debug info
      Serial.println("Connected to MQTT broker");
      
      // Once connected, publish an announcement to the ToHost/#deviceID# topic

      snprintf(topic, 32, "ToHost/%s", deviceID);
      snprintf(msg, 64, "CONNECTED", deviceID);
      MQTTclient.publish(topic, msg);
      
      // Subscribe to topics meant for this device
      snprintf(topic, 32, "ToDevice/%s", deviceID);
      
      MQTTclient.subscribe(topic);
      
      // Subscribe to topics meant for all devices
      MQTTclient.subscribe("ToDevice/All");
    }
    else {
      // Debug info why connection could not be made
      Serial.print("Connection to MQTT server failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" trying again in 5 seconds");
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
  // Call the main loop to check for and publish messages
  MQTTclient.loop();
}


void publish(char* message){

  snprintf(topic, 32, "ToHost/%s", deviceID);
  MQTTclient.publish(topic, message);
}

