#include <TeensyID.h>
#include <Arduino.h>
#include <ParagonMQTT.h>

//Egyptian Pillar One

#include <math.h>
#include <OctoWS2811.h>
#include "functions.h"
#include "color_temp_lookup.h"
#include "rgb_888.h"

// Literals
// Set the pillar to program here (1, 2, 3, or 4)
#define PILLAR                    2

#if PILLAR == 1
const char* deviceID = "PillarOne";
#elif PILLAR == 2
const char* deviceID = "PillarTwo";
#elif PILLAR == 3
const char* deviceID = "PillarThree";
#elif PILLAR == 4
const char* deviceID = "PillarFour";
#else
#error "Invalid PILLAR value. Set PILLAR to 1, 2, 3, or 4."
#endif

// Fire sim - 5.5" between panels, 35" from high panel to bowl
#define NUMBER_OF_HEAT_CELLS      80
#define NUMBER_OF_BOWL_HEAT_CELLS 32
#define NUMBER_OF_PANELS          3
#define NUMBER_OF_BURNS           4

#define LEDS_PER_PANEL            18
#define LOW_PANEL_HEIGHT          1
#define MID_PANEL_HEIGHT          26
#define HIGH_PANEL_HEIGHT         52
#define BOWL_HEIGHT               2
#define BURN_OFFSET               3   // Add this to make fire sim look better when burning at the bottom

// Glow fade in/out
#define GLOW_FADE_IN_RATE         2
#define GLOW_FADE_OUT_RATE        5

// LEDs
#define LEDS_PER_SIDE             72
#define LEDS_IN_BOWL              32
#define NUMBER_OF_SIDES           4
#define COLOR_ORDER               GRB
#define CHIPSET                   WS2811
#define FRAMES_PER_SECOND         60
#define BRIGHTNESS                255
#define NUM_STRIPS                9
const int numPins = 9;

#if PILLAR == 1
byte pinList[numPins] = {5, 6, 38, 39, 18, 17, 7, 11, 12};
#elif PILLAR == 2
byte pinList[numPins] = {17, 21, 6, 5, 11, 10, 23, 16, 12};
#elif PILLAR == 3
byte pinList[numPins] = {6, 5, 38, 39, 18, 21, 10, 7, 12};
#elif PILLAR == 4
byte pinList[numPins] = {5, 6, 10, 11, 18, 17, 38, 39, 12};
#else
#error "Invalid PILLAR value. Set PILLAR to 1, 2, 3, or 4."
#endif

const int ledsPerStrip = 72;
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

uint16_t rand16seed;

#define AUDIO_BAUD                115200
// Sensor pins per pillar (A1/B1/C1/D1 = top, A2/B2/C2/D2 = middle, A3/B3/C3/D3 = bottom)
#if PILLAR == 1
#define SENSOR_A1_PIN             4
#define SENSOR_A2_PIN             3
#define SENSOR_A3_PIN             2
#define SENSOR_B1_PIN             14
#define SENSOR_B2_PIN             15
#define SENSOR_B3_PIN             16
#define SENSOR_C1_PIN             19
#define SENSOR_C2_PIN             20
#define SENSOR_C3_PIN             21
#define SENSOR_D1_PIN             9
#define SENSOR_D2_PIN             8
#define SENSOR_D3_PIN             10
#elif PILLAR == 2
#define SENSOR_A1_PIN             19
#define SENSOR_A2_PIN             20
#define SENSOR_A3_PIN             18
#define SENSOR_B1_PIN             4
#define SENSOR_B2_PIN             3
#define SENSOR_B3_PIN             2
#define SENSOR_C1_PIN             9
#define SENSOR_C2_PIN             8
#define SENSOR_C3_PIN             7
#define SENSOR_D1_PIN             14
#define SENSOR_D2_PIN             15
#define SENSOR_D3_PIN             41
#elif PILLAR == 3
#define SENSOR_A1_PIN             4
#define SENSOR_A2_PIN             3
#define SENSOR_A3_PIN             2
#define SENSOR_B1_PIN             14
#define SENSOR_B2_PIN             15
#define SENSOR_B3_PIN             16
#define SENSOR_C1_PIN             19
#define SENSOR_C2_PIN             20
#define SENSOR_C3_PIN             17
#define SENSOR_D1_PIN             9
#define SENSOR_D2_PIN             8
#define SENSOR_D3_PIN             11
#elif PILLAR == 4
#define SENSOR_A1_PIN             4
#define SENSOR_A2_PIN             3
#define SENSOR_A3_PIN             2
#define SENSOR_B1_PIN             9
#define SENSOR_B2_PIN             8
#define SENSOR_B3_PIN             7
#define SENSOR_C1_PIN             19
#define SENSOR_C2_PIN             20
#define SENSOR_C3_PIN             21
#define SENSOR_D1_PIN             14
#define SENSOR_D2_PIN             15
#define SENSOR_D3_PIN             16
#else
#error "Invalid PILLAR value. Set PILLAR to 1, 2, 3, or 4."
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
  PILLARS_STATE,
  SOLVED_STATE,
};

enum
{
  IGNITE_LOW_A_SOUND_INDEX,
  IGNITE_LOW_B_SOUND_INDEX,
  IGNITE_LOW_C_SOUND_INDEX,
  IGNITE_LOW_D_SOUND_INDEX,
  IGNITE_MID_A_SOUND_INDEX,
  IGNITE_MID_B_SOUND_INDEX,
  IGNITE_MID_C_SOUND_INDEX,
  IGNITE_MID_D_SOUND_INDEX,
  IGNITE_HIGH_A_SOUND_INDEX,
  IGNITE_HIGH_B_SOUND_INDEX,
  IGNITE_HIGH_C_SOUND_INDEX,
  IGNITE_HIGH_D_SOUND_INDEX,
  IGNITE_BOWL_SOUND_INDEX,
  IGNITE_ALL_SOUND_INDEX,
  NUMBER_OF_IGNITE_SOUNDS,
  BURNING_SOUND_INDEX = NUMBER_OF_IGNITE_SOUNDS,
  NUMBER_OF_SOUNDS,
};

#define MIN_SOUND_MILLIS 500UL

// Pillars numbers in room with sides shown:
// _____________
// |  3    4   |
// |           |
// |     C     |
// |  2       B 1 D |
// |               A    |
// ---------------------
//
// Handprints (pillar, side, height*): 1B3, 2B3, 3A1
// Pillars (pillar, side, height*): 1D1, 1C2, 1C1, 1B1, 2A1, 2A2, 2B2, 2C1, 3D1, 3D3, 3C2, 3B3, 4B2, 4B3, 4A2, 4D3
// *Height: 1 = High, 2 = Mid, 3 = Low

// Map from sensor pins to positions
const struct 
{
  boolean handprint_solution;    
  boolean pillar_solution;
  uint8_t pin;
  uint16_t panel_height;
  uint8_t ignite_sound_index;
  uint8_t side_index;
  uint8_t height_index;  
} sensor_info[] =
{
  #if PILLAR == 1
  { false, true,  SENSOR_A1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_A_SOUND_INDEX, 0, 2 },  
  { false, true,  SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, true,  SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, false, SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { false, false, SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, false, SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, false, SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, true,  SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, false, SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { true,  false, SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
  #elif PILLAR == 2
  { false, true,  SENSOR_A1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_A_SOUND_INDEX, 0, 2 },  
  { false, true,  SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, true,  SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, false, SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { true,  false, SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, true,  SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, false, SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, false, SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, false, SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { false, false, SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
  #elif PILLAR == 3
  { true,  false, SENSOR_A1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_A_SOUND_INDEX, 0, 2 },  
  { false, false, SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, false, SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, false, SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { false, true,  SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, false, SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, true,  SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, true,  SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, false, SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { false, true,  SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
  #elif PILLAR == 4
  { false, false, SENSOR_A1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_A_SOUND_INDEX, 0, 2 },  
  { false, false, SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, false, SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, false, SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { false, true,  SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, false, SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, true,  SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, false, SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, true,  SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { false, true,  SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
  #endif
};
#define NUMBER_OF_SENSORS (sizeof(sensor_info) / sizeof(sensor_info[0]))

// Globals
extern const rgb_888_t color_temp_to_rgb[];



static uint32_t last_sound_millis;
static boolean burning_sound_active;


//Setting up Arrays
typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_t;

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
                   uint16_t *panel_heights, 
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
  heat[panel_heights[i]] += burn[i];
  }
}

inline const char* soundWordForIgniteIndex(uint8_t index)
{
  switch(index)
  {
    case IGNITE_LOW_A_SOUND_INDEX:
    case IGNITE_LOW_B_SOUND_INDEX:
    case IGNITE_LOW_C_SOUND_INDEX:
    case IGNITE_LOW_D_SOUND_INDEX:
      return "bottom";

    case IGNITE_MID_A_SOUND_INDEX:
    case IGNITE_MID_B_SOUND_INDEX:
    case IGNITE_MID_C_SOUND_INDEX:
    case IGNITE_MID_D_SOUND_INDEX:
      return "middle";

    case IGNITE_HIGH_A_SOUND_INDEX:
    case IGNITE_HIGH_B_SOUND_INDEX:
    case IGNITE_HIGH_C_SOUND_INDEX:
    case IGNITE_HIGH_D_SOUND_INDEX:
      return "top";

    case IGNITE_BOWL_SOUND_INDEX:
      return "bowl";

    case IGNITE_ALL_SOUND_INDEX:
      return "burning";

    default:
      return "burning";
  }
}

inline void sendAudioCommand(const char *word)
{
  Serial7.println(word);
  Serial.print("AUDIO -> ");
  Serial.println(word);
}

static void onState1Action(const char *value)
{
  (void)value;
  onState1();
}

static void onState2Action(const char *value)
{
  (void)value;
  onState2();
}

static void onState3Action(const char *value)
{
  (void)value;
  onState3();
}

static void onState4Action(const char *value)
{
  (void)value;
  onState4();
}

void registerMqttActions()
{
  registerAction("state1", onState1Action);
  registerAction("state2", onState2Action);
  registerAction("state3", onState3Action);
  registerAction("state4", onState4Action);
}

void setup() 
{
uint8_t side;
uint16_t i;

// Set up debug serial and sound card serial
Serial.begin(115200);
Serial7.begin(AUDIO_BAUD);

puzzle_state = WAITING_STATE;


leds.begin();
leds.show();
burning_sound_active = false;
last_sound_millis = 0;

// Set up general I/O
pinMode(POWER_LED_PIN, OUTPUT);


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
  digitalWrite(POWER_LED_PIN, HIGH);
  networkSetup();
  mqttSetup();
  registerMqttActions();
}

// Updates the state of the puzzle as a function of input from sensors and other puzzles
void updatePuzzleState(puzzleState_t *state, uint32_t current_millis)
{
  (void)current_millis;
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

if(PILLAR == 1 && puzzle_state == PILLARS_STATE){
  color.r = rgb.r;
  color.g = 0;
  color.b = 0;
}
else if(PILLAR == 2 && puzzle_state == PILLARS_STATE){
  color.r = rgb.b;
  color.g = rgb.g;
  color.b = rgb.r;
}
else if(PILLAR == 3 && puzzle_state == PILLARS_STATE){
  color.r = rgb.g;
  color.g = rgb.r;
  color.b = rgb.b;
}
else if(PILLAR == 4 && puzzle_state == PILLARS_STATE){
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
  // Each side has two mirrored strips: (A1,A2), (B1,B2), (C1,C2), (D1,D2)
  int strip1 = side * 2;
  int strip2 = strip1 + 1;
  leds.setPixel((ledsPerStrip * strip1) + ledIndex, color.r, color.g, color.b);
  leds.setPixel((ledsPerStrip * strip2) + ledIndex, color.r, color.g, color.b);
}

void setColor_bowl(int height_index, int addLed, rgb_t color) {
  const int bowlStripIndex = 8;
  leds.setPixel((ledsPerStrip * bowlStripIndex) + height_index + addLed, color.r, color.g, color.b);
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
  Serial.println(puzzle_state);
  }
  sendDataMQTT();
  
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
      burn_heights[burn_index] = sensor_info[i].panel_height + BURN_OFFSET;
      switch(puzzle_state)
        {          
          case HANDS_STATE:
            if(sensor_info[i].handprint_solution)
              {
              if(sensors[i] == IR_ACTIVATED)
                {
                glow[side][sensor_info[i].height_index] = qadd8(glow[side][sensor_info[i].height_index], GLOW_FADE_IN_RATE);
                      
                }
              else
                {
                glow[side][sensor_info[i].height_index] = qsub8(glow[side][sensor_info[i].height_index], GLOW_FADE_OUT_RATE);
                               
                }
              }
            break;       
          
          case PILLARS_STATE:
            // State 3: side sections ignite only while their sensor is active.
            if(sensors[i] == IR_ACTIVATED)
              {
              burn[burn_index++] = randnorm(normal_fire.mean, normal_fire.stddev);
              ignite_sounds[sensor_info[i].ignite_sound_index] = true;
              }
            break;
  
          case SOLVED_STATE:
            fire_params = tall_fire;
            if(sensor_info[i].panel_height == LOW_PANEL_HEIGHT)
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
      for(j = sensor_info[i].panel_height; j < sensor_info[i].panel_height + LEDS_PER_PANEL; j++)
        {
        if(puzzle_state == HANDS_STATE){
          brightness = (uint16_t)glow[side][sensor_info[i].height_index];
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
      if(puzzle_state == PILLARS_STATE 
      || puzzle_state == SOLVED_STATE)
        {
        ignite_sounds[IGNITE_BOWL_SOUND_INDEX] = true;
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
      setColor_bowl(BOWL_HEIGHT, a, color);
      }
  

// Process sound effects via Serial7 words for Raspberry Pi audio controller.
is_something_burning = false;
for(i = 0; i < NUMBER_OF_IGNITE_SOUNDS; i++)
  {
  is_something_burning |= ignite_sounds[i];

  if(!last_ignite_sounds[i] && ignite_sounds[i] && (last_sound_millis + MIN_SOUND_MILLIS <= current_millis))
    {
    sendAudioCommand(soundWordForIgniteIndex(i));
    last_sound_millis = current_millis;
    burning_sound_active = false;
    break;
    }
  }

if(is_something_burning)
  {
  if(!burning_sound_active && (last_sound_millis + MIN_SOUND_MILLIS <= current_millis))
    {
    sendAudioCommand("burning");
    last_sound_millis = current_millis;
    burning_sound_active = true;
    }
  }
else
  {
  burning_sound_active = false;
  }
memcpy(last_ignite_sounds, ignite_sounds, sizeof(last_ignite_sounds));

leds.show();
}

void onState1(){
  puzzle_state = WAITING_STATE;
  sendImmediateMQTT("state1");
}

void onState2(){
  puzzle_state = HANDS_STATE;
  sendImmediateMQTT("state2");
}

void onState3(){
  puzzle_state = PILLARS_STATE;
  sendImmediateMQTT("state3");
}

void onState4(){
  puzzle_state = SOLVED_STATE;
  sendImmediateMQTT("state4");
}
