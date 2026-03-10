#include <FastLED.h>

#define LED_PIN_1     25
#define LED_PIN_2     26
#define LED_PIN_3     27
#define LED_PIN_4     28
#define LED_PIN_5     29
#define LED_PIN_6     30
#define LED_PIN_7     31
#define LED_PIN_8     32
#define POWER_LED     13

#define COLOR_ORDER RGB
#define CHIPSET     WS2812B
#define NUM_LEDS    12
#define BRIGHTNESS  10
#define FRAMES_PER_SECOND 15

CRGB ledsA[NUM_LEDS];
CRGB ledsB[NUM_LEDS];
CRGB ledsC[NUM_LEDS];
CRGB ledsD[NUM_LEDS];
CRGB ledsE[NUM_LEDS];
CRGB ledsF[NUM_LEDS];
CRGB ledsG[NUM_LEDS];
CRGB ledsH[NUM_LEDS];

CHSV currentPalette;

void setup() {
  FastLED.addLeds<CHIPSET, LED_PIN_1, COLOR_ORDER>(ledsA, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_2, COLOR_ORDER>(ledsB, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_3, COLOR_ORDER>(ledsC, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_4, COLOR_ORDER>(ledsD, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_5, COLOR_ORDER>(ledsE, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_6, COLOR_ORDER>(ledsF, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_7, COLOR_ORDER>(ledsG, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_8, COLOR_ORDER>(ledsH, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

  pinMode(POWER_LED, OUTPUT);

  digitalWrite(POWER_LED, HIGH);
}

void loop()
{
  CRGB darkcolor  = CHSV(30, 255, 200); // pure hue, three-quarters brightness
    CRGB lightcolor = CHSV(30, 255, 50); // half 'whitened', full brightness
    currentPalette = CRGBPalette16( darkcolor, lightcolor);
  Fire2012(); // run simulation frame
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}



#define COOLING  75

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 120


void Fire2012() // defines a new function
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for ( int i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < SPARKING ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( int j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte index = scale8( heat[j], 100);
  
      ledsA[j] = CHSV( 255, 100, 100); //random8(0, 200)
      ledsB[j] = CHSV( 255, 100, 100);
      ledsC[j] = CHSV( 255, 100, 100);
      ledsD[j] = CHSV( 255, 100, 100);
      ledsE[j] = CHSV( 255, 100, 100);
      ledsF[j] = CHSV( 255, 100, 100);
      ledsG[j] = CHSV( 255, 100, 100);
      ledsH[j] = CHSV( 255, 100, 100);
    }
}


