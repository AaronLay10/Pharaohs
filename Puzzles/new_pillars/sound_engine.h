/******************************************************************************
 * SOUND ENGINE — Audio command dispatch to Raspberry Pi via Serial7
 *
 * Sends keyword strings ("bottom", "middle", "top", "bowl", "burning", "stop")
 * to a Raspberry Pi audio player. Handles edge-detection (new ignitions),
 * sound interruption, minimum play duration, and background looping.
 ******************************************************************************/
#ifndef SOUND_ENGINE_H
#define SOUND_ENGINE_H

#include <Arduino.h>
#include "pillar_config.h"

// ─── Sound Indices ──────────────────────────────────────────────────────────
enum SoundIndex {
  SND_IGNITE_BOTTOM_A,
  SND_IGNITE_BOTTOM_B,
  SND_IGNITE_BOTTOM_C,
  SND_IGNITE_BOTTOM_D,
  SND_IGNITE_MIDDLE_A,
  SND_IGNITE_MIDDLE_B,
  SND_IGNITE_MIDDLE_C,
  SND_IGNITE_MIDDLE_D,
  SND_IGNITE_TOP_A,
  SND_IGNITE_TOP_B,
  SND_IGNITE_TOP_C,
  SND_IGNITE_TOP_D,
  SND_IGNITE_BOWL,
  NUM_IGNITE_SOUNDS,
  SND_BURNING = NUM_IGNITE_SOUNDS,
  NUM_SOUNDS,
};

#define MIN_SOUND_MILLIS      500UL

// ─── Lookup: sensor position → sound index base ─────────────────────────────
// Formula: (position * 4) + side gives the ignite sound index
// BOTTOM=0..3, MIDDLE=4..7, TOP=8..11, BOWL=12

inline uint8_t sensorToSoundIndex(uint8_t side, uint8_t position)
{
  return (position * NUMBER_OF_SIDES) + side;
}

inline const char* soundKeyword(uint8_t index)
{
  if(index <= SND_IGNITE_BOTTOM_D) return "bottom";
  if(index <= SND_IGNITE_MIDDLE_D) return "middle";
  if(index <= SND_IGNITE_TOP_D)    return "top";
  if(index == SND_IGNITE_BOWL)     return "bowl";
  if(index == SND_BURNING)         return "burning";
  return "bottom";
}

// ─── Sound State ────────────────────────────────────────────────────────────
static uint32_t last_sound_millis;
static bool     last_ignite_state[NUM_IGNITE_SOUNDS];

void soundEngineInit()
{
  AUDIO_SERIAL.begin(AUDIO_BAUD);
  last_sound_millis = 0;
  memset(last_ignite_state, 0, sizeof(last_ignite_state));
}

void sendSoundKeyword(const char *keyword)
{
  AUDIO_SERIAL.println(keyword);
}

// Process sound for the current frame.
// ignite_active[]: which ignite sounds are active this frame
// is_pillar_state: true if we're in PILLAR_STATE (slightly different logic)
void processSounds(bool ignite_active[], bool is_pillar_state, uint32_t now)
{
  bool is_playing = (last_sound_millis + MIN_SOUND_MILLIS > now);
  bool something_burning = false;

  for(uint8_t i = 0; i < NUM_IGNITE_SOUNDS; i++)
  {
    something_burning |= ignite_active[i];

    // Detect leading edge — new ignition at a position
    if(!last_ignite_state[i] && ignite_active[i])
    {
      // Interrupt current sound if playing
      if(is_playing)
      {
        sendSoundKeyword("stop");
        last_sound_millis = now - MIN_SOUND_MILLIS;
      }
      sendSoundKeyword(soundKeyword(i));
      last_sound_millis = now;
      is_playing = true;
      break;  // Only trigger one new sound per frame
    }
  }

  // Background burning loop (not in PILLAR_STATE — that state handles its own)
  if(!is_pillar_state)
  {
    if(something_burning && !is_playing)
    {
      sendSoundKeyword("burning");
      last_sound_millis = now;
    }
    else if(!something_burning && is_playing)
    {
      sendSoundKeyword("stop");
      last_sound_millis = now - MIN_SOUND_MILLIS;
    }
  }

  // Save state for next frame edge detection
  memcpy(last_ignite_state, ignite_active, sizeof(last_ignite_state));
}

#endif // SOUND_ENGINE_H
