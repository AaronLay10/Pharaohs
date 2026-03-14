/******************************************************************************
 * PILLAR CONFIGURATION — Runtime auto-detection via Teensy MAC address
 *
 * One firmware for all four pillars. On boot, the Teensy reads its own MAC
 * address and matches it against the known pillar MACs below. Everything
 * (deviceID, sensors, puzzle solutions, pillar hue) is selected at runtime.
 *
 * TO SET UP:
 *   1. Flash this firmware to each Teensy
 *   2. Open Serial Monitor — it will print the MAC address
 *   3. Copy each MAC into the PILLAR_MACS table below
 *   4. Re-flash once — all four pillars now auto-identify
 ******************************************************************************/
#ifndef PILLAR_CONFIG_H
#define PILLAR_CONFIG_H

#include <Arduino.h>
#include <TeensyID.h>

// ─── Common Constants ───────────────────────────────────────────────────────
#define NUM_PILLARS           4
#define NUMBER_OF_SIDES       4
#define LEDS_PER_STRIP        72
#define LEDS_PER_SIDE         144    // 2 mirrored strips per side
#define LEDS_IN_BOWL          32
#define NUMBER_OF_PANELS      3
#define LEDS_PER_PANEL        18

// Section start offsets within each 72-LED strip
#define BOTTOM_SECTION_START  2
#define MIDDLE_SECTION_START  26
#define TOP_SECTION_START     52

// LED strip indices
#define NUM_STRIPS            9
#define STRIP_A1              0
#define STRIP_A2              1
#define STRIP_B1              2
#define STRIP_B2              3
#define STRIP_C1              4
#define STRIP_C2              5
#define STRIP_D1              6
#define STRIP_D2              7
#define STRIP_BOWL            8

// Sensor constants
#define NUMBER_OF_SENSORS     12
#define IR_ACTIVATED          0
#define IR_NOT_ACTIVATED      1

// Position indices
#define POS_BOTTOM            0
#define POS_MIDDLE            1
#define POS_TOP               2

// Raspberry Pi Audio Serial
#define AUDIO_SERIAL          Serial7
#define AUDIO_BAUD            115200

// Power LED
#define POWER_LED_PIN         13

// ─── Sensor Info Structure ──────────────────────────────────────────────────
struct SensorInfo {
  uint8_t  pin;
  uint8_t  side_index;        // 0=A, 1=B, 2=C, 3=D
  uint8_t  position_index;    // POS_BOTTOM, POS_MIDDLE, POS_TOP
  uint16_t led_section_start;
  bool     handprint_solution;
  bool     pillar_solution;
};

// ─── Pillar Hue (for State 3 tinting) ───────────────────────────────────────
struct PillarHue {
  uint8_t r_scale;
  uint8_t g_scale;
  uint8_t b_scale;
};

// ─── MAC Address Table ──────────────────────────────────────────────────────
// Fill in the last 3 bytes of each Teensy's MAC address after first boot.
// Teensy 4.1 MACs are 04:E9:E5:XX:XX:XX — only the last 3 bytes vary.
//
// To find a Teensy's MAC:
//   Flash this firmware, open Serial Monitor, look for:
//   "MAC Address: 04:E9:E5:XX:XX:XX"

struct PillarMAC {
  uint8_t mac[6];
};

const PillarMAC PILLAR_MACS[NUM_PILLARS] = {
  {{ 0x04, 0xE9, 0xE5, 0x10, 0x93, 0x1C }},  // Pillar 1
  {{ 0x04, 0xE9, 0xE5, 0x1B, 0x22, 0x02 }},  // Pillar 2
  {{ 0x04, 0xE9, 0xE5, 0x1B, 0x21, 0xF7 }},  // Pillar 3
  {{ 0x04, 0xE9, 0xE5, 0x10, 0x93, 0x1B }},  // Pillar 4
};

// ─── Device Names ───────────────────────────────────────────────────────────
const char* PILLAR_NAMES[NUM_PILLARS] = {
  "PillarOne", "PillarTwo", "PillarThree", "PillarFour"
};

// ─── Pillar Hues ────────────────────────────────────────────────────────────
const PillarHue PILLAR_HUES[NUM_PILLARS] = {
  {1, 0, 0},  // Pillar 1 — Red
  {0, 0, 1},  // Pillar 2 — Blue
  {0, 1, 0},  // Pillar 3 — Green
  {1, 1, 0},  // Pillar 4 — Yellow
};

// ─── LED Strip Pins Per Pillar ──────────────────────────────────────────────
// Order: A1, A2, B1, B2, C1, C2, D1, D2, Bowl
// Pin mapping: old pin 40 → 23, old pin 41 → 22 (FastLED max pin = 39)
const uint8_t STRIP_PINS[NUM_PILLARS][NUM_STRIPS] = {
  { 5,  6, 22, 23, 18, 17,  7, 11, 12},  // Pillar 1 (was 41→22, 40→23)
  {17, 21,  6,  5, 11, 10, 23, 16, 12},  // Pillar 2 (was 40→23)
  { 6,  5, 23, 22, 18, 21, 10,  7, 12},  // Pillar 3 (was 40→23, 41→22)
  { 5,  6, 10, 11, 18, 17, 23, 22, 12},  // Pillar 4 (was 40→23, 41→22)
};

// ─── Sensor Maps Per Pillar ─────────────────────────────────────────────────
// Order: AT, AM, AB, BT, BM, BB, CT, CM, CB, DT, DM, DB
const SensorInfo SENSOR_MAPS[NUM_PILLARS][NUMBER_OF_SENSORS] = {
  // ── Pillar 1 ────────────────────────────────────────────────────────────
  {
    { 4,  0, POS_TOP,    TOP_SECTION_START,    false, false},  // AT
    { 3,  0, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // AM
    { 2,  0, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // AB
    {14,  1, POS_TOP,    TOP_SECTION_START,    false, true },  // BT
    {15,  1, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // BM
    {16,  1, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // BB
    {19,  2, POS_TOP,    TOP_SECTION_START,    false, true },  // CT
    {20,  2, POS_MIDDLE, MIDDLE_SECTION_START, false, true },  // CM
    {21,  2, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // CB
    { 9,  3, POS_TOP,    TOP_SECTION_START,    false, true },  // DT
    { 8,  3, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // DM
    {10,  3, POS_BOTTOM, BOTTOM_SECTION_START, true,  false},  // DB
  },
  // ── Pillar 2 ────────────────────────────────────────────────────────────
  {
    {19,  0, POS_TOP,    TOP_SECTION_START,    false, true },  // AT
    {20,  0, POS_MIDDLE, MIDDLE_SECTION_START, false, true },  // AM
    {18,  0, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // AB
    { 4,  1, POS_TOP,    TOP_SECTION_START,    false, true },  // BT
    { 3,  1, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // BM
    { 2,  1, POS_BOTTOM, BOTTOM_SECTION_START, true,  false},  // BB
    { 9,  2, POS_TOP,    TOP_SECTION_START,    false, true },  // CT
    { 8,  2, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // CM
    { 7,  2, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // CB
    {14,  3, POS_TOP,    TOP_SECTION_START,    false, false},  // DT
    {15,  3, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // DM
    {41,  3, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // DB
  },
  // ── Pillar 3 ────────────────────────────────────────────────────────────
  {
    { 4,  0, POS_TOP,    TOP_SECTION_START,    true,  false},  // AT
    { 3,  0, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // AM
    { 2,  0, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // AB
    {14,  1, POS_TOP,    TOP_SECTION_START,    false, false},  // BT
    {15,  1, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // BM
    {16,  1, POS_BOTTOM, BOTTOM_SECTION_START, false, true },  // BB
    {19,  2, POS_TOP,    TOP_SECTION_START,    false, false},  // CT
    {20,  2, POS_MIDDLE, MIDDLE_SECTION_START, false, true },  // CM
    {17,  2, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // CB
    { 9,  3, POS_TOP,    TOP_SECTION_START,    false, true },  // DT
    { 8,  3, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // DM
    {11,  3, POS_BOTTOM, BOTTOM_SECTION_START, false, true },  // DB
  },
  // ── Pillar 4 ────────────────────────────────────────────────────────────
  {
    { 4,  0, POS_TOP,    TOP_SECTION_START,    false, false},  // AT
    { 3,  0, POS_MIDDLE, MIDDLE_SECTION_START, false, true },  // AM
    { 2,  0, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // AB
    { 9,  1, POS_TOP,    TOP_SECTION_START,    false, false},  // BT
    { 8,  1, POS_MIDDLE, MIDDLE_SECTION_START, false, true },  // BM
    { 7,  1, POS_BOTTOM, BOTTOM_SECTION_START, false, true },  // BB
    {19,  2, POS_TOP,    TOP_SECTION_START,    false, false},  // CT
    {20,  2, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // CM
    {21,  2, POS_BOTTOM, BOTTOM_SECTION_START, false, false},  // CB
    {14,  3, POS_TOP,    TOP_SECTION_START,    false, false},  // DT
    {15,  3, POS_MIDDLE, MIDDLE_SECTION_START, false, false},  // DM
    {16,  3, POS_BOTTOM, BOTTOM_SECTION_START, false, true },  // DB
  },
};

// ─── Runtime State (set during setup) ───────────────────────────────────────
extern uint8_t pillarIndex;            // 0-3
extern const char* deviceID;
extern const SensorInfo* sensorMap;    // Points to SENSOR_MAPS[pillarIndex]
extern PillarHue pillarHue;

// ─── Auto-Detection Function ────────────────────────────────────────────────
// Returns pillar index (0-3) or 255 if MAC not found
uint8_t detectPillar()
{
  uint8_t mac[6];
  teensyMAC(mac);

  Serial.printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  for(uint8_t p = 0; p < NUM_PILLARS; p++)
  {
    if(memcmp(mac, PILLAR_MACS[p].mac, 6) == 0)
    {
      Serial.printf("Identified as Pillar %d (%s)\n", p + 1, PILLAR_NAMES[p]);
      return p;
    }
  }

  Serial.println("WARNING: MAC not recognized! Defaulting to Pillar 1.");
  Serial.println("Add this MAC to PILLAR_MACS[] in pillar_config.h");
  return 0;
}

#endif // PILLAR_CONFIG_H
