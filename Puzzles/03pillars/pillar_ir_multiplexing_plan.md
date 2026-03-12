# Pharaohs Escape Room — Pillar IR Multiplexing Plan

## Goal
Use direct pillar-to-pillar sync wiring to time-multiplex only the interference-prone IR sensor banks while keeping the rest of the firmware and puzzle behavior as intact as possible.

---

## Final Design Decisions

### Multiplex timing
- **ON time per side:** `50 ms`
- Full cycle: `100 ms`
- Leaders generate a continuous metronome during active states

### Active states
Multiplexing is active only in:
- `HANDS_STATE`
- `PILLAR_STATE`

All sensors are ignored in:
- `WAITING_STATE`
- `SOLVED_STATE`

### Interfering pillar pairs
#### Pair 1
- **Leader:** Pillar 1
- **Follower:** Pillar 4
- **Multiplexed sides:**
  - Pillar 1 → Side A
  - Pillar 4 → Side C

#### Pair 2
- **Leader:** Pillar 2
- **Follower:** Pillar 3
- **Multiplexed sides:**
  - Pillar 2 → Side C
  - Pillar 3 → Side A

### Direct sync wiring
For each pair:
- **Sync signal:** pin `1`
- **Common ground:** required

### Sync meaning
- Leader drives the sync pin continuously in active states
- **HIGH** = leader multiplexed side is active
- Follower uses the **inverse** of the sync pin

### Follower recovery behavior
- If sync edges stop arriving for too long, follower falls back to its own local inverse timer
- If sync returns, follower **immediately re-locks** to current pin level

### Sensor behavior
- Only the 3 sensors on the affected facing side are multiplexed
- During active window: read those sensors every loop
- During inactive window: hold last known state
- All other 9 sensors on that pillar remain normal

---

## Firmware Changes

## 1. Add multiplex definitions
Place near the other `#define` blocks.

```cpp
// Multiplex sync
#define MUX_SYNC_PIN              1
#define MUX_ON_TIME_MS            50UL
#define MUX_LOST_TIMEOUT_MS       200UL   // 4 missed half-cycles = sync considered lost

#if PILLAR == 1 || PILLAR == 2
  #define IS_MUX_LEADER           1
#else
  #define IS_MUX_LEADER           0
#endif
```

---

## 2. Add `multiplexed` to the sensor table struct
Change this:

```cpp
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
```

To this:

```cpp
const struct
{
  boolean handprint_solution;    
  boolean pillar_solution;
  boolean multiplexed;
  uint8_t pin;
  uint16_t led_section_start;
  uint8_t ignite_sound_index;
  uint8_t side_index;
  uint8_t position_index;  
} sensor_info[] =
```

---

## 3. Mark the multiplexed sensors in `sensor_info[]`

### Pillar 1 — multiplex Side A
```cpp
#if PILLAR == 1
  { false, false, true,  SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },  
  { false, false, true,  SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },  
  { false, false, true,  SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, true,  false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },  
  { true,  false, false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },
  
  { false, true,  false, SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, true,  false, SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, true,  false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
```

### Pillar 2 — multiplex Side C
```cpp
#elif PILLAR == 2
  { false, true,  false, SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },  
  { false, true,  false, SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, true,  false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },  
  { true,  false, false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },
  
  { false, true,  true,  SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, false, true,  SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },  
  { false, false, true,  SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, false, false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
```

### Pillar 3 — multiplex Side A
```cpp
#elif PILLAR == 3
  { true,  false, true,  SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },  
  { false, false, true,  SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },  
  { false, false, true,  SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, false, false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },  
  { false, true,  false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },
  
  { false, false, false, SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, true,  false, SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, true,  false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },  
  { false, true,  false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
```

### Pillar 4 — multiplex Side C
```cpp
#elif PILLAR == 4
  { false, false, false, SENSOR_AT_PIN, TOP_SECTION_START,    IGNITE_TOP_A_SOUND_INDEX,    0, TOP_POSITION_INDEX },  
  { false, true,  false, SENSOR_AM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_A_SOUND_INDEX, 0, MIDDLE_POSITION_INDEX },  
  { false, false, false, SENSOR_AB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_A_SOUND_INDEX, 0, BOTTOM_POSITION_INDEX },

  { false, false, false, SENSOR_BT_PIN, TOP_SECTION_START,    IGNITE_TOP_B_SOUND_INDEX,    1, TOP_POSITION_INDEX },
  { false, true,  false, SENSOR_BM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_B_SOUND_INDEX, 1, MIDDLE_POSITION_INDEX },  
  { false, true,  false, SENSOR_BB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_B_SOUND_INDEX, 1, BOTTOM_POSITION_INDEX },
  
  { false, false, true,  SENSOR_CT_PIN, TOP_SECTION_START,    IGNITE_TOP_C_SOUND_INDEX,    2, TOP_POSITION_INDEX },
  { false, false, true,  SENSOR_CM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_C_SOUND_INDEX, 2, MIDDLE_POSITION_INDEX },  
  { false, false, true,  SENSOR_CB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_C_SOUND_INDEX, 2, BOTTOM_POSITION_INDEX },

  { false, false, false, SENSOR_DT_PIN, TOP_SECTION_START,    IGNITE_TOP_D_SOUND_INDEX,    3, TOP_POSITION_INDEX },
  { false, false, false, SENSOR_DM_PIN, MIDDLE_SECTION_START, IGNITE_MIDDLE_D_SOUND_INDEX, 3, MIDDLE_POSITION_INDEX },  
  { false, true,  false, SENSOR_DB_PIN, BOTTOM_SECTION_START, IGNITE_BOTTOM_D_SOUND_INDEX, 3, BOTTOM_POSITION_INDEX }
#endif
```

---

## 4. Add multiplex globals
Place with the other globals.

```cpp
static boolean mux_active = true;              // whether THIS pillar's multiplexed side is active
static boolean mux_using_wire = false;         // follower locked to leader wire
static uint32_t mux_start_millis = 0;          // local metronome start
static uint32_t mux_last_edge_millis = 0;      // follower edge watchdog
static uint8_t mux_last_sync_level = HIGH;     // last observed wire state
```

---

## 5. Add helper functions
Place above `setup()` or above `loop()`.

```cpp
void resetMultiplexClock()
{
  mux_start_millis = millis();
  mux_last_edge_millis = mux_start_millis;
}

boolean isMultiplexEnabledForState()
{
  return (puzzle_state == HANDS_STATE || puzzle_state == PILLAR_STATE);
}

boolean computeLocalLeaderActive(uint32_t now)
{
  return (((now - mux_start_millis) % (2UL * MUX_ON_TIME_MS)) < MUX_ON_TIME_MS);
}

void updateMultiplexState(uint32_t now)
{
  if(!isMultiplexEnabledForState())
  {
    mux_active = true;
#if IS_MUX_LEADER
    digitalWrite(MUX_SYNC_PIN, LOW);
#endif
    return;
  }

#if IS_MUX_LEADER
  mux_active = computeLocalLeaderActive(now);
  digitalWrite(MUX_SYNC_PIN, mux_active ? HIGH : LOW);
#else
  uint8_t sync_level = digitalRead(MUX_SYNC_PIN);

  if(sync_level != mux_last_sync_level)
  {
    mux_last_sync_level = sync_level;
    mux_last_edge_millis = now;
    mux_using_wire = true;
  }

  if((now - mux_last_edge_millis) > MUX_LOST_TIMEOUT_MS)
  {
    mux_using_wire = false;
  }

  if(mux_using_wire)
  {
    mux_active = (sync_level == LOW);
  }
  else
  {
    mux_active = !computeLocalLeaderActive(now);
  }
#endif
}
```

---

## 6. Configure pin 1 in `setup()`
Add under “Set up general I/O”.

```cpp
#if IS_MUX_LEADER
  pinMode(MUX_SYNC_PIN, OUTPUT);
  digitalWrite(MUX_SYNC_PIN, LOW);
#else
  pinMode(MUX_SYNC_PIN, INPUT_PULLUP);
  mux_last_sync_level = digitalRead(MUX_SYNC_PIN);
  mux_last_edge_millis = millis();
  mux_using_wire = true;
#endif

resetMultiplexClock();
```

---

## 7. Replace raw sensor reads in `loop()`
Replace this:

```cpp
// Read the sensors
for( i = 0; i < NUMBER_OF_SENSORS; i++ )
{
  sensors[i] = digitalRead(sensor_info[i].pin);
}
```

With this:

```cpp
updateMultiplexState(current_millis);

// Read the sensors
for(i = 0; i < NUMBER_OF_SENSORS; i++)
{
  if(sensor_info[i].multiplexed)
  {
    if(mux_active)
    {
      sensors[i] = digitalRead(sensor_info[i].pin);
    }
    // else hold last known state
  }
  else
  {
    sensors[i] = digitalRead(sensor_info[i].pin);
  }
}
```

---

## 8. Reset multiplex timing on state entry
Patch the state functions.

```cpp
void onState1(){
  startPillars = false;
  puzzle_state = WAITING_STATE;
#if IS_MUX_LEADER
  digitalWrite(MUX_SYNC_PIN, LOW);
#endif
}

void onState2(){
  startPillars = true;
  puzzle_state = HANDS_STATE;
  resetMultiplexClock();
#if !IS_MUX_LEADER
  mux_using_wire = true;
  mux_last_sync_level = digitalRead(MUX_SYNC_PIN);
  mux_last_edge_millis = millis();
#endif
  publish("CONNECTED");
}

void onState3(){
  startPillars = true;
  puzzle_state = PILLAR_STATE;
  resetMultiplexClock();
#if !IS_MUX_LEADER
  mux_using_wire = true;
  mux_last_sync_level = digitalRead(MUX_SYNC_PIN);
  mux_last_edge_millis = millis();
#endif
}

void onState4(){
  startPillars = true;
  puzzle_state = SOLVED_STATE;
#if IS_MUX_LEADER
  digitalWrite(MUX_SYNC_PIN, LOW);
#endif
}
```

---

## 9. Tighten the active loop state check
Current code:

```cpp
if(startPillars && puzzle_state != WAITING_STATE)
```

Change to:

```cpp
if(startPillars && (puzzle_state == HANDS_STATE || puzzle_state == PILLAR_STATE))
```

This matches the final behavior where states 1 and 4 ignore all sensors.

---

## Resulting behavior

### Leaders
- Pillar 1 multiplexes Side A
- Pillar 2 multiplexes Side C
- pin 1 is HIGH for 50 ms when leader bank is active
- pin 1 is LOW for 50 ms when follower bank should be active

### Followers
- Pillar 4 uses inverse of Pillar 1 sync for Side C
- Pillar 3 uses inverse of Pillar 2 sync for Side A
- if sync stops toggling for 200 ms, follower runs local inverse timing
- when sync edges return, follower immediately re-locks

### Everything else
- fire, glow, sound, and MQTT logic remain mostly unchanged
- only the sensor acquisition layer becomes multiplex-aware

---

## Recommended test order
1. Bench test **one pair only**
2. Put an LED + resistor on the sync line temporarily
3. Verify clean **50 ms HIGH / 50 ms LOW** behavior
4. Print `mux_active` and sync level on both pillars to Serial
5. Test the real sensors
6. Reboot the leader while follower runs and verify recovery

---

## Notes
- Pin 1 is also `Serial1 TX` on Teensy 4.1, so do not use `Serial1` elsewhere if pin 1 is your sync line.
- Followers should use `INPUT_PULLUP` on the sync pin.
- Leaders should drive the sync pin as `OUTPUT`.
- The direct pair sync line is handling timing, not MQTT.
- MQTT remains for puzzle state and orchestration.

