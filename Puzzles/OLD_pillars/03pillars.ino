#include <TeensyID.h>
#include <Arduino.h>
#include <NativeEthernet.h>
#include <PubSubClient.h>

uint8_t mac[6];

//Egyptian Pillar One

#include <math.h>
#include <OctoWS2811.h>
#include <Adafruit_Soundboard.h>
#include "functions.h"
#include "color_temp_lookup.h"
#include "rgb_888.h"

// Literals
// Set the pillar to program here (1, 2, 3, or 4)
#define PILLAR                    2

// Fire sim - 5.5" between panels, 35" from high panel to bowl
#define NUMBER_OF_HEAT_CELLS      80
#define NUMBER_OF_BOWL_HEAT_CELLS 40
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
#define LEDS_IN_BOWL              40
#define NUMBER_OF_SIDES           4
#define COLOR_ORDER               GRB
#define CHIPSET                   WS2811
#define FRAMES_PER_SECOND         60
#define BRIGHTNESS                255
#define NUM_STRIPS                5
const int numPins = 5;
byte pinList[numPins] = {2,3,4,5,6};
const int ledsPerStrip = 72;
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];
const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

uint16_t rand16seed;

// Pin assignments

#define LED_STRIP_SIDE_A_PIN      2  // Data for side A LED strip
#define LED_STRIP_SIDE_B_PIN      3 // Data for side B LED strip
#define LED_STRIP_SIDE_C_PIN      4  // Data for side C LED strip
#define LED_STRIP_SIDE_D_PIN      5  // Data for side D LED strip
#define LED_STRIP_BOWL_PIN        6 // Data for bowl LED strip
//Soundboard Pins
#define SOUND_RESET_PIN           36  // Assert reset on Adafruit SFX board
#define SOUND_RX_PIN              35  // Rx for Serial3 from Adafruit SFX board
#define SOUND_TX_PIN              34  // Tx for Serial3 to Adafruit SFX board
#define SOUND_ACT_PIN             33  // Check for sound activation on Adafruit SFX board
//Sensor Pins
#define SENSOR_A1_PIN             10 // Sensor for panel 1 (top) on side A
#define SENSOR_A2_PIN             11 // Sensor for panel 2 (middle) on side A
#define SENSOR_A3_PIN             12 // Sensor for panel 3 (bottom) on side A
#define SENSOR_B1_PIN             9 // Sensor for panel 1 (top) on side B
#define SENSOR_B2_PIN             8 // Sensor for panel 2 (middle) on side B
#define SENSOR_B3_PIN             7 // Sensor for panel 3 (bottom) on side B
#define SENSOR_C1_PIN             24 // Sensor for panel 1 (top) on side C
#define SENSOR_C2_PIN             25 // Sensor for panel 2 (middle) on side C
#define SENSOR_C3_PIN             26 // Sensor for panel 3 (bottom) on side C
#define SENSOR_D1_PIN             27 // Sensor for panel 1 (top) on side D
#define SENSOR_D2_PIN             28 // Sensor for panel 2 (middle) on side D
#define SENSOR_D3_PIN             29 // Sensor for panel 3 (bottom) on side D

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
  HANDPRINTS_ENABLED_STATE,
  BOWL_IGNITED_STATE,
  PILLAR_ENABLED_STATE,
  PILLAR_SOLVED_STATE,
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
  EASTER_EGG_SOUND_INDEX,
  NUMBER_OF_SOUNDS,
};

#define MIN_SOUND_MILLIS 500UL

// The order of this array must correspond to the above sound index enum
const char *sound_names[NUMBER_OF_SOUNDS] =
{
  "LOW     WAV",
  "LOW     WAV",
  "LOW     WAV",
  "LOW     WAV",
  "MID     WAV",
  "MID     WAV",
  "MID     WAV",
  "MID     WAV",
  "HIGH    WAV",
  "HIGH    WAV",
  "HIGH    WAV",
  "HIGH    WAV",
  "BOWL    WAV",
  "LOUD    WAV",
  "BURNING WAV"};

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
  { false, false, SENSOR_A1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_A_SOUND_INDEX, 0, 2 },  
  { false, false, SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, true,  SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, false, SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { true,  false, SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, true,  SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, true,  SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, true,  SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, false, SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { false, false, SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
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
  { false, true,  SENSOR_A2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_A_SOUND_INDEX,  0, 1 },  
  { false, false, SENSOR_A3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_A_SOUND_INDEX,  0, 0 },

  { false, false, SENSOR_B1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_B_SOUND_INDEX, 1, 2 },
  { false, true,  SENSOR_B2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_B_SOUND_INDEX,  1, 1 },  
  { false, true,  SENSOR_B3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_B_SOUND_INDEX,  1, 0 },
  
  { false, false, SENSOR_C1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_C_SOUND_INDEX, 2, 2 },
  { false, false, SENSOR_C2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_C_SOUND_INDEX,  2, 1 },  
  { false, false, SENSOR_C3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_C_SOUND_INDEX,  2, 0 },

  { false, false, SENSOR_D1_PIN, HIGH_PANEL_HEIGHT, IGNITE_HIGH_D_SOUND_INDEX, 3, 2 },
  { false, false, SENSOR_D2_PIN, MID_PANEL_HEIGHT,  IGNITE_MID_D_SOUND_INDEX,  3, 1 },  
  { false, true,  SENSOR_D3_PIN, LOW_PANEL_HEIGHT,  IGNITE_LOW_D_SOUND_INDEX,  3, 0 }
  #endif
};
#define NUMBER_OF_SENSORS (sizeof(sensor_info) / sizeof(sensor_info[0]))

// Globals
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial8, NULL, SOUND_RESET_PIN);
boolean sfx_detected;
char publishDetail[50] = {0};

extern const rgb_888_t color_temp_to_rgb[];



static uint32_t last_sound_millis;


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

void setup() 
{
uint8_t side;
uint16_t i;

int state = 1;

// Set up debug serial and sound card serial
Serial.begin(115200);
Serial8.begin(9600);

teensyMAC(mac);
Serial.printf("String MAC Address: %s\n", teensyMAC());

puzzle_state = HANDPRINTS_ENABLED_STATE;


leds.begin();
leds.show();

// Ensure sound board has good power, then reset the sound board
delay(2000);
sfx_detected = sfx.reset();

// Set up general I/O

pinMode(SOUND_ACT_PIN, INPUT_PULLUP);
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
  case BOWL_IGNITED_STATE:
    sfx.playTrack((char *)sound_names[13]);
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

if(PILLAR == 1 && puzzle_state == PILLAR_ENABLED_STATE){
  color.r = rgb.r;
  color.g = 0;
  color.b = 0;
}
else if(PILLAR == 2 && puzzle_state == PILLAR_ENABLED_STATE){
  color.r = rgb.b;
  color.g = rgb.g;
  color.b = rgb.r;
}
else if(PILLAR == 3 && puzzle_state == PILLAR_ENABLED_STATE){
  color.r = rgb.g;
  color.g = rgb.r;
  color.b = rgb.b;
}
else if(PILLAR == 4 && puzzle_state == PILLAR_ENABLED_STATE){
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
 
  leds.setPixel((ledsPerStrip * side) + ledIndex, color.r, color.g, color.b);
  
}

void setColor_bowl(int height_index, int addLed, rgb_t color) {
  
  leds.setPixel(height_index + addLed, color.r, color.g, color.b);
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
if(startPillars){
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
  
is_sound_playing = (digitalRead(SOUND_ACT_PIN) == LOW) || (last_sound_millis + MIN_SOUND_MILLIS > current_millis);


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
          case HANDPRINTS_ENABLED_STATE:
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
          
          case PILLAR_ENABLED_STATE:
            if(sensors[i] == IR_ACTIVATED)
              {
              burn[burn_index++] = randnorm(normal_fire.mean, normal_fire.stddev);
              ignite_sounds[sensor_info[i].ignite_sound_index] = true;
              }
            break;
  
          case PILLAR_SOLVED_STATE:
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
        if(puzzle_state == HANDPRINTS_ENABLED_STATE){
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
      if(puzzle_state == BOWL_IGNITED_STATE 
      || puzzle_state == PILLAR_ENABLED_STATE 
      || puzzle_state == PILLAR_SOLVED_STATE)
        {
        if(puzzle_state == BOWL_IGNITED_STATE)
        {
        ignite_sounds[IGNITE_BOWL_SOUND_INDEX] = true;
        }
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
      setColor_bowl(290 + BOWL_HEIGHT, a, color);
      }
  

// Process sound effects if in Pillar Enabled State
if(sfx_detected && puzzle_state == PILLAR_ENABLED_STATE)
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
        sfx.stop();
        last_sound_millis = current_millis - MIN_SOUND_MILLIS;
        Serial.println("Stop sound due to different burn position");
        }
      Serial.print("Play ");
      Serial.println((char *)sound_names[i]);
      sfx.playTrack((char *)sound_names[i]);
      last_sound_millis = current_millis;
      is_sound_playing = true;
      break;
      }
    }
  }


// Process sound effects (except easter egg or Pillar Enabled)
if(sfx_detected && puzzle_state != PILLAR_ENABLED_STATE && puzzle_state != BOWL_IGNITED_STATE)
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
        sfx.stop();
        last_sound_millis = current_millis - MIN_SOUND_MILLIS;
        Serial.println("Stop sound due to different burn position");
        }
      Serial.print("Play ");
      Serial.println((char *)sound_names[i]);
      sfx.playTrack((char *)sound_names[i]);
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
    Serial.println((char *)sound_names[i]);
    sfx.playTrack((char *)sound_names[BURNING_SOUND_INDEX]);
    last_sound_millis = current_millis;
    }
  else if(!is_something_burning && is_sound_playing)
    {
    sfx.stop();
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

void onSolveOne(){
  solveOne = true;
  puzzle_state = PILLAR_ENABLED_STATE;
}

void onResetOne(){
  puzzle_state = HANDPRINTS_ENABLED_STATE;
  publish("CONNECTED");
}

void onOverrideOne(){
  puzzle_state = BOWL_IGNITED_STATE;
}

void onSolveTwo(){
   puzzle_state = PILLAR_SOLVED_STATE;
}

void onResetTwo(){
  puzzle_state = PILLAR_ENABLED_STATE;
  publish("CONNECTED");
}

void onOverrideTwo(){
  puzzle_state = PILLAR_SOLVED_STATE;
}
