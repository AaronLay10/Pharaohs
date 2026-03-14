 ---                                                                                                                           
  03pillars.ino — Comprehensive Sketch Outline                                                                                  
                                                                                                                                
  1. Purpose                                                                                                                    
                                                                                                                                
  Controls 4 Egyptian pillar props in the "Return of the Pharaohs" escape room. Each pillar is an independent Teensy 4.1 running
   the same codebase, differentiated only by a #define PILLAR value (1–4). The pillars display fire/glow LED animations
  triggered by IR proximity sensors, play sound effects via a Raspberry Pi audio controller, and communicate with a central game
   server over MQTT.

  ---
  2. Hardware Per Pillar

  ┌───────────┬─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │ Component │                                                   Details                                                   │
  ├───────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ MCU       │ Teensy 4.1                                                                                                  │
  ├───────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ LED       │ 9 strips via OctoWS2811 DMA: 2 mirrored strips per side (4 sides × 2 = 8) + 1 bowl strip. Each strip = 72   │
  │ strips    │ WS2811 LEDs (GRB, 800kHz).                                                                                  │
  ├───────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ IR        │ 12 per pillar: 4 sides × 3 positions (Top/Middle/Bottom). Active-low (IR_ACTIVATED = 0), configured as      │
  │ sensors   │ INPUT_PULLUP.                                                                                               │
  ├───────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Audio     │ Raspberry Pi connected via Serial7 (pins 28/29, 115200 baud). Receives keyword commands ("bottom",          │
  │           │ "middle", "top", "bowl", "burning", "stop").                                                                │
  ├───────────┼─────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ Network   │ NativeEthernet via Teensy 4.1 built-in Ethernet jack. MQTT broker at 192.168.20.8:33002.                    │
  └───────────┴─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---
  3. Compile-Time Configuration (#define PILLAR 1-4)

  Each pillar value selects:
  - deviceID: "PillarOne" through "PillarFour" (MQTT identity)
  - pinList[]: 9 GPIO pins for the OctoWS2811 LED strips (different wiring per pillar)
  - Sensor pin assignments: 12 IR sensor GPIO pins (different wiring per pillar)
  - sensor_info[] table: Per-sensor metadata including:
    - handprint_solution flag — marks the 3 sensors that glow green in HANDS_STATE
    - pillar_solution flag — marks the 4 sensors that are the correct puzzle answer
    - pin — GPIO pin
    - led_section_start — which 18-LED section this sensor maps to (2, 26, or 52)
    - ignite_sound_index — which ignite sound to trigger
    - side_index — 0–3 (A–D)
    - position_index — BOTTOM/MIDDLE/TOP
  - Pillar color tint (in PILLAR_STATE): Pillar 1=Red, 2=Blue, 3=Green, 4=Yellow

  Puzzle Solutions (hardcoded in sensor_info)

  - Handprints (3 total across all pillars): 1DB, 2BB, 3AT
  - Pillar solutions (4 sensors per pillar):
    - Pillar 1: DT, CM, CT, BT
    - Pillar 2: AT, AM, BM, CT
    - Pillar 3: DT, DB, CM, BB
    - Pillar 4: BM, BB, AM, DB

  ---
  4. LED Layout

  Each side (A/B/C/D) has 144 LEDs total = 2 physical strips of 72, driven in mirror via setColor() which writes to both strips
  of a pair simultaneously.

  Each 72-LED strip is divided into 3 sections of 18 LEDs each:
  - Bottom: LEDs 2–19 (BOTTOM_SECTION_START = 2)
  - Middle: LEDs 26–43 (MIDDLE_SECTION_START = 26)
  - Top: LEDs 52–69 (TOP_SECTION_START = 52)

  The bowl is a separate 40-LED strip on strip index 8.

  ---
  5. State Machine (4 states, driven by MQTT)

  State transitions are not automatic — they are commanded externally by the game server sending MQTT actions. The
  updatePuzzleState() function exists but its switch/case body is empty (state transitions happen only via MQTT state1–state4
  actions).

  WAITING_STATE (State 1)

  - All LEDs off (blackout)
  - Sensors not read
  - publishDetail = "Waiting for Jackal and Ankh"
  - MQTT continues to be serviced

  HANDS_STATE (State 2)

  - Reads all 12 sensors every frame
  - Only sensors with handprint_solution = true produce a visual effect
  - Effect: green glow that fades in (rate 2) when sensor active, fades out (rate 5) when inactive
  - The glow fills the 18-LED section corresponding to that sensor's position
  - No sound effects
  - On entry: publishes "CONNECTED" via MQTT

  PILLAR_STATE (State 3)

  - Reads all 12 sensors every frame
  - Per-section fire simulation: Each of the 3 sections (18 LEDs) on each side runs an independent cellular-automaton fire
  - When a sensor is active, heat is injected at the section base (qadd8(heat, random(130,220)))
  - Fire color is tinted by pillar color via applyPillarColor() (reduces to single-hue intensity)
  - Bowl also runs continuous fire, pillar-tinted
  - Sound: Ignite sounds trigger on leading edge (sensor newly active), with stop/interrupt logic. Plays positional sounds
  ("bottom", "middle", "top", "bowl")

  SOLVED_STATE (State 4)

  - Sensors ignored (all cleared to inactive)
  - Full-strip fire: All 72 LEDs per side run as one continuous flame with auto-sparking at the base
  - Natural fire colors (no pillar tint — applyPillarColor() passes through when not PILLAR_STATE)
  - Bowl runs continuous fire, also natural colors
  - Continuous burning sound

  ---
  6. Fire Simulation Engine

  step_fire(uint8_t *heat, int start, int len, bool inject_spark)

  Classic cellular-automaton fire algorithm:
  1. Cool: Each cell loses random(0, SECTION_COOLING + 2) heat
  2. Drift up: heat[k] = (heat[k-1] + heat[k-2] + heat[k-2]) / 3 — weighted average pulling heat upward
  3. Auto-spark (if inject_spark): Random chance (SOLVED_SPARKING = 120) to add random(160, 255) heat at base cells

  heatColor(uint8_t temperature) → rgb_t

  Trilinear fire palette (FastLED-style):
  - 0–63 scaled: black → red (ramp red channel)
  - 64–127 scaled: red → yellow (ramp green channel)
  - 128–191 scaled: yellow → white (ramp blue channel)

  applyPillarColor(rgb_t c) → rgb_t

  Only active in PILLAR_STATE. Extracts max(r,g,b) as intensity, then maps to pillar hue:
  - Pillar 1: (intensity, 0, 0) — Red
  - Pillar 2: (0, 0, intensity) — Blue
  - Pillar 3: (0, intensity, 0) — Green
  - Pillar 4: (intensity, intensity, 0) — Yellow

  Tuning Constants

  - SECTION_COOLING = 2, SECTION_SPARKING = 100 (State 3 section fire)
  - SOLVED_COOLING = 1, SOLVED_SPARKING = 120 (State 4 tall fire)
  - GLOW_FADE_IN_RATE = 2, GLOW_FADE_OUT_RATE = 5 (State 2 glow)

  ---
  7. Sound System

  Audio is sent as keyword strings over Serial7 to a Raspberry Pi audio player.

  Sound indices (enum): 12 ignite sounds (Bottom/Middle/Top × sides A–D) + Bowl + Burning = 15 total.

  Logic:
  - Tracks last_ignite_sounds[] to detect leading edges (new ignition)
  - New ignition interrupts current sound ("stop" then new keyword)
  - Minimum 500ms between sound starts (MIN_SOUND_MILLIS)
  - In non-PILLAR states: plays background "burning" when something is active but initial sound finished
  - soundKeywordFromIndex() maps index ranges to keywords: "bottom", "middle", "top", "bowl", "burning"

  ---
  8. MQTT Integration (via ParagonMQTT library)

  - Subscribes to: ToDevice/PillarOne (or Two/Three/Four) and ToDevice/All
  - Publishes to: ToHost/PillarOne (etc.)
  - Registered actions: "state1" → "state4" — each calls onStateN() which sets puzzle_state, clears sensors, blacks out LEDs
  - Telemetry: sendDataMQTT() called every loop, publishes sensor hex dump (publishDetail) at 500ms rate limit
  - Broker: 192.168.20.8:33002

  ---
  9. Utility Functions

  - qadd8(a, b) — Saturating 8-bit add (caps at 255)
  - qsub8(a, b) — Saturating 8-bit subtract (floors at 0)
  - setColor(side, ledIndex, color) — Writes to both mirrored strips for a side
  - setColor_bowl(ledIndex, color) — Writes to bowl strip (strip index 8)
  - blackoutAllLeds() — Sets all pixels to black and calls leds.show()
  - clearAllSensorsToInactive() — Sets all sensor states to IR_NOT_ACTIVATED

  ---
  10. Associated Files

  ┌─────────────────────┬────────────────────────────────────────────────────────────────────────────────────────────────────┐
  │        File         │                                              Purpose                                               │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ rgb_888.h           │ Defines rgb_888_t struct (r, g, b uint8). Currently unused in main sketch (sketch defines its own  │
  │                     │ rgb_t).                                                                                            │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ color_temp_lookup.h │ Defines KELVIN_TO_IDX macro. Currently unused in main sketch.                                      │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ color_temp_lookup.c │ PROGMEM lookup table mapping color temperatures (1024K–20000K) to RGB values. Currently unused —   │
  │                     │ legacy from earlier fire rendering approach.                                                       │
  ├─────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ arduino.json        │ Build config: board=teensy:avr:teensy41, output=build/                                             │
  └─────────────────────┴────────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---
  11. Build System

  - Arduino CLI / VS Code with Arduino extension
  - Board: teensy:avr:teensy41
  - Builds per-pillar by setting #define PILLAR (1–4) before compile
  - Build artifacts stored in build/pillar1/ through build/pillar4/
  - Hex artifacts copied to build/hex_artifacts/

  ---
  12. Key Dependencies

  ┌────────────────┬──────────────────────────────────────────────────────────────────────────────────────────────────┐
  │    Library     │                                             Purpose                                              │
  ├────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ OctoWS2811     │ DMA-driven WS2811/WS2812 LED control for Teensy (up to 8+ parallel strips)                       │
  ├────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ ParagonMQTT    │ Custom library wrapping PubSubClient + NativeEthernet for Paragon escape room MQTT communication │
  ├────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ TeensyID       │ Provides hardware MAC address for Ethernet                                                       │
  ├────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ NativeEthernet │ Teensy 4.1 built-in Ethernet support (pulled in by ParagonMQTT)                                  │
  ├────────────────┼──────────────────────────────────────────────────────────────────────────────────────────────────┤
  │ PubSubClient   │ MQTT client (pulled in by ParagonMQTT)                                                           │
  └────────────────┴──────────────────────────────────────────────────────────────────────────────────────────────────┘

  ---
  13. Reproduction Notes for AI Agents

  To recreate this sketch from scratch:

  1. Start with the PILLAR conditional compilation pattern — single codebase, #ifndef PILLAR / #define PILLAR N / #endif, with
  #if PILLAR == N blocks for pin assignments, deviceID, and solution tables.
  2. Set up OctoWS2811 with 9 strips (8 side strips + 1 bowl), DMAMEM display buffer, drawingMemory buffer, GRB 800kHz config.
  The setColor() function writes to both strips of a mirrored pair.
  3. Build the sensor_info[] table — this is the core data structure. Each entry maps a sensor pin to its side, position, LED
  section, sound index, and solution flags. The table is fully compile-time constant.
  4. Implement the 4-state machine driven exclusively by MQTT state1–state4 actions (no auto-transitions).
  5. Fire simulation uses the classic cellular-automaton: cool, drift-up, spark. State 3 = per-section (18 LEDs, sensor-driven
  sparks, pillar-tinted). State 4 = full-strip (72 LEDs, auto-sparks, natural fire colors).
  6. Sound goes out Serial7 as plaintext keywords. Edge-detection on last_ignite_sounds[] array triggers new sounds. Background
  burning loops when initial sound finishes.
  7. MQTT telemetry runs every loop regardless of state. Sensor states are published as a hex-formatted string.
  8. The color_temp_lookup.c/h and rgb_888.h files are legacy — not currently used in the fire rendering pipeline. The active
  palette is heatColor().