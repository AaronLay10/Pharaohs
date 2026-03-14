/******************************************************************************
 * FIRE ENGINE — Cellular automaton fire simulation + enhanced fire palette
 *
 * Provides two fire modes:
 *   - Section fire (State 3): 18-LED sections driven by individual sensors
 *   - Full-strip fire (State 4): 72-LED tall flames with dynamic effects
 *
 * The State 4 fire uses a richer palette with deep reds, vivid oranges,
 * bright yellows, and occasional white-hot tips for a magical look.
 ******************************************************************************/
#ifndef FIRE_ENGINE_H
#define FIRE_ENGINE_H

#include <FastLED.h>
#include "pillar_config.h"

// ─── Fire Tuning ────────────────────────────────────────────────────────────

// State 3: Per-section fire (sensor-triggered)
#define SECTION_COOLING       2
#define SECTION_SPARKING      100

// State 4: Full-strip solved fire (auto-sparking, tall dramatic flames)
#define SOLVED_COOLING        1      // Low cooling = tall flames
#define SOLVED_SPARKING       140    // High base spark probability
#define SOLVED_EXTRA_SPARKS   3      // Number of extra spark injection points
#define SOLVED_WIND_CHANCE    30     // % chance of wind gust per frame

// Bowl fire
#define BOWL_COOLING          2
#define BOWL_SPARKING         130

// Glow (State 2)
#define GLOW_FADE_IN_RATE     2
#define GLOW_FADE_OUT_RATE    5

// ─── Heat Arrays ────────────────────────────────────────────────────────────

extern uint8_t side_heat[NUMBER_OF_SIDES][LEDS_PER_STRIP];
extern uint8_t bowl_heat[LEDS_IN_BOWL];
extern uint8_t glow_brightness[NUMBER_OF_SIDES][NUMBER_OF_PANELS];

// ─── Fire Palette ───────────────────────────────────────────────────────────
//
// A 256-entry gradient that goes:
//   0:   Black (cold, no fire)
//   ~50: Deep maroon/crimson (ember glow)
//   ~90: Pure red (fire base)
//   ~130: Red-orange (hot flames)
//   ~170: Bright orange (intense fire)
//   ~200: Orange-yellow (flame tips)
//   ~230: Bright yellow (peak heat)
//   ~245: Yellow-white (white-hot tips)
//   255: Pure white (sparks)
//
// This produces richer, more magical fire than the basic trilinear palette.

// Defined as a FastLED gradient palette
DEFINE_GRADIENT_PALETTE(FirePalette_gp) {
    0,    0,   0,   0,    // Black
   20,   15,   0,   0,    // Barely visible deep red ember
   50,   80,   0,   0,    // Dark crimson
   80,  180,   5,   0,    // Deep red
  100,  255,  20,   0,    // Pure red-orange glow
  120,  255,  60,   0,    // Rich orange
  145,  255, 100,   0,    // Bright orange
  170,  255, 140,   0,    // Orange-amber
  190,  255, 180,   0,    // Amber-yellow
  210,  255, 210,  10,    // Warm yellow
  230,  255, 240,  40,    // Bright yellow
  245,  255, 250, 130,    // Yellow-white hot
  255,  255, 255, 200,    // White-hot spark tips
};

// ─── Functions ──────────────────────────────────────────────────────────────

void fireEngineInit();

// Core fire step: advances the cellular automaton for heat[start..start+len-1]
void stepFire(uint8_t *heat_array, int start, int len,
              uint8_t cooling, uint8_t sparking, bool inject_spark);

// Enhanced State 4 fire step with wind gusts and multi-point sparking
void stepSolvedFire(uint8_t *heat_array, int start, int len, bool inject_spark);

// Map heat value (0-255) to fire color using the rich gradient palette
CRGB heatToColor(uint8_t heat_val);

// Apply pillar hue tint (for State 3 only)
CRGB applyPillarTint(CRGB color);

// ─── Implementation ─────────────────────────────────────────────────────────

uint8_t side_heat[NUMBER_OF_SIDES][LEDS_PER_STRIP];
uint8_t bowl_heat[LEDS_IN_BOWL];
uint8_t glow_brightness[NUMBER_OF_SIDES][NUMBER_OF_PANELS];

static CRGBPalette16 firePalette;

void fireEngineInit()
{
  memset(side_heat, 0, sizeof(side_heat));
  memset(bowl_heat, 0, sizeof(bowl_heat));
  memset(glow_brightness, 0, sizeof(glow_brightness));
  firePalette = FirePalette_gp;
}

void stepFire(uint8_t *heat_array, int start, int len,
              uint8_t cooling, uint8_t sparking, bool inject_spark)
{
  // 1. Cool every cell
  for(int i = start; i < start + len; i++)
  {
    uint8_t cool_amount = random8(0, ((cooling * 10) / len) + 2);
    heat_array[i] = qsub8(heat_array[i], cool_amount);
  }

  // 2. Drift heat upward (higher index = physically higher on strip)
  for(int k = start + len - 1; k >= start + 2; k--)
  {
    heat_array[k] = (
      (uint16_t)heat_array[k - 1] +
      (uint16_t)heat_array[k - 2] +
      (uint16_t)heat_array[k - 2]
    ) / 3;
  }

  // 3. Optionally ignite new sparks at the base
  if(inject_spark && random8() < sparking)
  {
    int y = start + random8(min(5, len));
    heat_array[y] = qadd8(heat_array[y], random8(160, 255));
  }
}

void stepSolvedFire(uint8_t *heat_array, int start, int len, bool inject_spark)
{
  (void)inject_spark; // Always injects sparks — parameter kept for API consistency
  // 1. Cool with very low cooling for tall, dramatic flames
  for(int i = start; i < start + len; i++)
  {
    uint8_t cool_amount = random8(0, ((SOLVED_COOLING * 10) / len) + 2);
    heat_array[i] = qsub8(heat_array[i], cool_amount);
  }

  // 2. Drift heat upward with slight randomization for organic movement
  for(int k = start + len - 1; k >= start + 2; k--)
  {
    // Occasionally vary the drift ratio for flicker
    if(random8() < 40)
    {
      // Slightly different weighting creates wavering
      heat_array[k] = (
        (uint16_t)heat_array[k - 1] +
        (uint16_t)heat_array[k - 1] +
        (uint16_t)heat_array[k - 2]
      ) / 3;
    }
    else
    {
      heat_array[k] = (
        (uint16_t)heat_array[k - 1] +
        (uint16_t)heat_array[k - 2] +
        (uint16_t)heat_array[k - 2]
      ) / 3;
    }
  }

  // 3. Multi-point spark injection at the base for a wide, roaring fire
  for(int s = 0; s < SOLVED_EXTRA_SPARKS; s++)
  {
    if(random8() < SOLVED_SPARKING)
    {
      int y = start + random8(min(7, len));
      heat_array[y] = qadd8(heat_array[y], random8(180, 255));
    }
  }

  // 4. Wind gusts — occasionally push heat sideways/upward in bursts
  if(random8(100) < SOLVED_WIND_CHANCE)
  {
    // Pick a random mid-section and boost it
    int gust_pos = start + random8(len / 3, (len * 2) / 3);
    heat_array[gust_pos] = qadd8(heat_array[gust_pos], random8(60, 120));
  }

  // 5. Occasional bright ember that rises high
  if(random8() < 15)
  {
    int ember_pos = start + len - random8(1, min(10, len));
    heat_array[ember_pos] = qadd8(heat_array[ember_pos], random8(80, 160));
  }
}

CRGB heatToColor(uint8_t heat_val)
{
  // Scale through the fire palette with interpolation
  return ColorFromPalette(firePalette, heat_val, 255, LINEARBLEND);
}

CRGB applyPillarTint(CRGB color)
{
  // Extract brightness as the max channel
  uint8_t intensity = max(color.r, max(color.g, color.b));

  CRGB out;
  out.r = pillarHue.r_scale ? intensity : 0;
  out.g = pillarHue.g_scale ? intensity : 0;
  out.b = pillarHue.b_scale ? intensity : 0;
  return out;
}

#endif // FIRE_ENGINE_H
