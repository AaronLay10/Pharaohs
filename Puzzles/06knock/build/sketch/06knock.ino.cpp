#include <Arduino.h>
#line 1 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
#include <OctoWS2811.h>
#include <ParagonMQTT.h>

const char *deviceID = "Knock";
const char *roomID = "Pharaohs";

const int POWERLED = 13;

// Leds for the Circle
const int ledsPerStrip = 16;
const int numPins = 1;
byte pinList[numPins] = {19};

// These buffers need to be large enough for all the pixels.
// The total number of pixels is "ledsPerStrip * numPins".
// Each pixel needs 3 bytes, so multiply by 3.  An "int" is
// 4 bytes, so divide by 4.  The array is created using "int"
// so the compiler will align it to 32 bit memory.
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

// Pin definitions
#define KNOCK_SENSOR A0 // Piezo sensor on pin 0 connected to ground with 1M pulldown resistor

#define MAGLOCK 7

const int threshold = 100;         // Minimum signal from the piezo to register as a knock
const int rejectValue = 25;        // If an individual knock is off by this percentage of a knock we don't unlock..
const int averageRejectValue = 25; // If the average timing of the knocks is off by this percent we don't unlock.
const int knockFadeTime = 150;     // milliseconds we allow a knock to fade before we listen for another one. (Debounce timer.)

const int maximumKnocks = 8;    // Maximum number of knocks to listen for.
const int knockComplete = 1500; // Longest time to wait for a knock before we assume that it's finished.

// Heartbeat timing
const unsigned long heartbeatInterval = 5000; // Send heartbeat every 5 seconds
unsigned long lastHeartbeat = 0;

// Variables.
int secretCode[maximumKnocks] = {75, 25, 100, 75, 25, 50};
int knockReadings[maximumKnocks]; // When someone knocks this array fills with delays between knocks.
int knockSensorValue = 0;         // Last reading of the knock sensor.

#line 48 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void setup();
#line 79 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void loop();
#line 106 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void listenToSecretKnock();
#line 161 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void knockAccepted();
#line 173 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
boolean validateKnock();
#line 235 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void onOverride();
#line 246 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void onReset();
#line 48 "/Users/aaron/Documents/Paragon/Return of the Pharaohs/Pharaohs-Puzzle-Code/06knock/06knock.ino"
void setup()
{
  Serial.begin(115200);
  Serial.println("Program start.");

  pinMode(MAGLOCK, OUTPUT);
  pinMode(POWERLED, OUTPUT);

  digitalWrite(MAGLOCK, HIGH);
  digitalWrite(POWERLED, HIGH);

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  // LED Setup

  leds.begin();
  leds.show();

  // Setup the ethernet network connection
  networkSetup();
  // Setup the MQTT service
  mqttSetup();

  // Register MQTT action handlers
  registerAction("override", [](const char *value)
                 { onOverride(); });
  registerAction("reset", [](const char *value)
                 { onReset(); });
}

void loop()
{
  sendDataMQTT();

  // Send heartbeat if enough time has passed and no other message is pending
  unsigned long currentTime = millis();
  if (currentTime - lastHeartbeat >= heartbeatInterval && publishDetail[0] == '\0')
  {
    strcpy(publishDetail, "Ready for Knock");
    lastHeartbeat = currentTime;
  }

  // Listen for any knock at all.
  knockSensorValue = analogRead(KNOCK_SENSOR);
  /*if(knockSensorValue > 10){
    Serial.println(knockSensorValue);
  } */

  if (knockSensorValue >= threshold)
  {
    listenToSecretKnock();
  }

  leds.show();
}

// Records the timing of knocks.
void listenToSecretKnock()
{
  Serial.println("knock starting");

  int i = 0;
  // First lets reset the listening array.
  for (i = 0; i < maximumKnocks; i++)
  {
    knockReadings[i] = 0;
  }

  int currentKnockNumber = 0; // Incrementer for the array.
  int startTime = millis();   // Reference for when this knock started.
  int now = millis();

  delay(knockFadeTime); // wait for this peak to fade before we listen to the next one.

  do
  {
    // listen for the next knock or wait for it to timeout.
    knockSensorValue = analogRead(KNOCK_SENSOR);
    if (knockSensorValue >= threshold)
    { // got another knock...
      // record the delay time.
      Serial.println("knock.");
      now = millis();
      knockReadings[currentKnockNumber] = now - startTime;

      currentKnockNumber++; // increment the counter
      startTime = now;      // and reset our timer for the next knock

      delay(knockFadeTime); // again, a little delay to let the knock decay.
    }

    now = millis();

    // did we timeout or run out of knocks?
  } while ((now - startTime < knockComplete) && (currentKnockNumber < maximumKnocks));

  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", knockReadings[0], knockReadings[1], knockReadings[2], knockReadings[3], knockReadings[4], knockReadings[5], knockReadings[6], knockReadings[7]);
  Serial.println(publishDetail);

  // we've got our knock recorded, lets see if it's valid
  if (validateKnock() == true)
  {
    knockAccepted();
  }
  else
  {
    Serial.println("Knock Failed.");
    strcpy(publishDetail, "Knock Incorrect");
  }
}

// Knock accepted. Trigger MAGLOCK
void knockAccepted()
{
  Serial.println("Good");
  digitalWrite(MAGLOCK, LOW);
  for (int j = 0; j < ledsPerStrip; j++)
  {
    leds.setPixel(j, 0, 255, 0);
  }
  leds.show();
  strcpy(publishDetail, "solved");
}

boolean validateKnock()
{
  Serial.println("Validating Knock...");
  int i = 0;

  // simplest check first: Did we get the right number of knocks?
  int currentKnockCount = 0;
  int secretKnockCount = 0;
  int maxKnockInterval = 0; // We use this later to normalize the times.

  for (i = 0; i < maximumKnocks; i++)
  {
    if (knockReadings[i] > 0)
    {
      currentKnockCount++;
    }
    if (secretCode[i] > 0)
    { // todo: precalculate this.
      secretKnockCount++;
    }

    if (knockReadings[i] > maxKnockInterval)
    { // collect normalization data while we're looping.
      maxKnockInterval = knockReadings[i];
    }
  }

  // If we're recording a new knock, save the info and get out of here.

  // if (currentKnockCount != secretKnockCount){
  // return false;
  //}

  /*  Now we compare the relative intervals of our knocks, not the absolute time between them.
      (ie: if you do the same pattern slow or fast it should still open the door.)
      This makes it less picky, which while making it less secure can also make it
      less of a pain to use if you're tempo is a little slow or fast.
  */
  int totaltimeDifferences = 0;
  int timeDiff = 0;
  for (i = 0; i < maximumKnocks; i++)
  { // Normalize the times
    knockReadings[i] = map(knockReadings[i], 0, maxKnockInterval, 0, 100);
    Serial.println(knockReadings[i]);

    timeDiff = abs(knockReadings[i] - secretCode[i]);
    if (timeDiff > rejectValue)
    { // Individual value too far out of whack
      return false;
    }
    totaltimeDifferences += timeDiff;
  }

  // It can also fail if the whole thing is too inaccurate.
  if (totaltimeDifferences / secretKnockCount > averageRejectValue)
  {
    return false;
  }

  return true;
}

void onOverride()
{
  digitalWrite(MAGLOCK, LOW);
  Serial.println("OVERRIDE");
  for (int j = 0; j < ledsPerStrip; j++)
  {
    leds.setPixel(j, 0, 255, 0);
  }
  leds.show();
}

void onReset()
{
  digitalWrite(MAGLOCK, HIGH);
  Serial.println("RESET!!!");
  for (int j = 0; j < ledsPerStrip; j++)
  {
    leds.setPixel(j, 0, 0, 0);
  }
  leds.show();
}

