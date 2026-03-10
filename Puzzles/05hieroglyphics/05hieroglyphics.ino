#include <OctoWS2811.h>
#include <TeensyID.h>

uint8_t mac[6];

#define MAGLOCK_OUT   32

//Pins for LED Strips
#define GREENCIRCLE   24
#define GLYPH_A       25
#define GLYPH_B       26
#define GLYPH_C       27
#define GLYPH_D       39
#define GLYPH_E       40
#define GLYPH_F       41

//Pins for the Main Amulet spot that lights the Blue Circles
#define SLOTMAIN_0    2
#define SLOTMAIN_1    3
#define SLOTMAIN_2    4
#define SLOTMAIN_3    5
#define SLOTMAIN_4    6
#define SLOTMAIN_5    7

//Pins for the Final position of the Amulets to solve the puzzle
#define SLOT0         8
#define SLOT1         9
#define SLOT2         10
#define SLOT3         17
#define SLOT4         18
#define SLOT5         19

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;

int ledIndex[7] = {16,48,0,64,32,80,96};

#define NUM_GLYPH_STRIPS 6
#define CIRCLE_START_LED 96

int state = 0;

#define INTERVAL 500
int previousMillis = 0;

//OctoWS2811 Setup
const int numPins = 7;
byte pinList[numPins] = {GLYPH_A,GLYPH_B,GLYPH_C,GLYPH_D,GLYPH_E,GLYPH_F,GREENCIRCLE};
const int ledsPerStrip = 16;
DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];
const int config = WS2811_GRB | WS2811_800kHz;
OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

char publishDetail[50] = {0};

void setup() {

  leds.begin();
  leds.show();

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());
  
  pinMode(SLOTMAIN_0, INPUT_PULLUP);
  pinMode(SLOTMAIN_1, INPUT_PULLUP);
  pinMode(SLOTMAIN_2, INPUT_PULLUP);
  pinMode(SLOTMAIN_3, INPUT_PULLUP);
  pinMode(SLOTMAIN_4, INPUT_PULLUP);
  pinMode(SLOTMAIN_5, INPUT_PULLUP);
  
  pinMode(SLOT0, INPUT_PULLUP);
  pinMode(SLOT1, INPUT_PULLUP);
  pinMode(SLOT2, INPUT_PULLUP);
  pinMode(SLOT3, INPUT_PULLUP);
  pinMode(SLOT4, INPUT_PULLUP);
  pinMode(SLOT5, INPUT_PULLUP);

  pinMode(MAGLOCK_OUT, OUTPUT);

  //using a transistor to control maglock.
  digitalWrite(MAGLOCK_OUT, HIGH);

  Serial.begin(115200);

    //Setup the ethernet network connection
 ethernetSetup();
  //Setup the MQTT service
 mqttSetup();
}

void loop() {

 if(state == 1){
    unsigned long currentMillisMqtt = millis();
    if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(SLOT1),digitalRead(SLOT0),digitalRead(SLOT2),digitalRead(SLOT3),digitalRead(SLOT4),digitalRead(SLOT5));
    publish(publishDetail);
    Serial.println(publishDetail);
    mqttLoop();
    return;
    }
  }



  if(state == 0){
  uint8_t sensorLineBit;
  unsigned long currentMillis = millis();
                
  sensorLineBit = digitalRead(SLOT0) 
                | digitalRead(SLOT1) << 1 
                | digitalRead(SLOT2) << 2 
                | digitalRead(SLOT3) << 3 
                | digitalRead(SLOT4) << 4 
                | digitalRead(SLOT5) << 5;
 
    lightCircle();
  }

 

unsigned long currentMillisMqtt = millis();
if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(SLOT1),digitalRead(SLOT0),digitalRead(SLOT2),digitalRead(SLOT3),digitalRead(SLOT4),digitalRead(SLOT5));
    publish(publishDetail);
    Serial.println(publishDetail);
    mqttLoop();
  }
}
void lightCircle(){
  uint8_t i;
  byte sensorMainBit;

  //Read sensors and create a value to apply to a variable
  sensorMainBit = digitalRead(SLOTMAIN_0) 
                | digitalRead(SLOTMAIN_1) << 1 
                | digitalRead(SLOTMAIN_2) << 2 
                | digitalRead(SLOTMAIN_3) << 3 
                | digitalRead(SLOTMAIN_4) << 4 
                | digitalRead(SLOTMAIN_5) << 5
                | (1 << 6)
                | (1 << 7);

   for(i = 0; i < NUM_GLYPH_STRIPS; i++) {
    //Serial.print("For Sensor #: ");
    //Serial.print(i);
    
    // Light up a strip if it corresponds to the only main slot detected
    // Example: If SLOTMAIN_3 is low, sensorMainBit is 0xF7 (247) (11110111), ~sensorMainBit is 8,
    // so when i is 3, (1 << 3) = 8, 8 XOR 8 == 0 and we will set white. If any other main
    // slots are detected, the result of the XOR will be non-zero and we will set black.
    if(((~sensorMainBit & 0xff) ^ (1 << i)) == 0) {
      Serial.print("Set LED strip ");
      Serial.print(i);
      Serial.println(" to white");
      for (int j = ledIndex[i] ; j < ledIndex[i] + ledsPerStrip; j++){
        Serial.print(ledIndex[i]);
        //Serial.print(":");
        //Serial.println(j);
        leds.setPixel(j, 255,255,255);
      }
    }
    else {
      //Serial.println(" to black");
      
      for (int j = ledIndex[i] ; j < ledIndex[i] + ledsPerStrip; j++){
        leds.setPixel(j, 0,0,0);
      }

    }
   }

   leds.show();
}

void onSolve() {
  digitalWrite(MAGLOCK_OUT, LOW);
  state = 1;
  for (int j = CIRCLE_START_LED; j < CIRCLE_START_LED + ledsPerStrip; j++){
        leds.setPixel(j, 0,0,255);
      }
  leds.show();
}

void onReset() {
  digitalWrite(MAGLOCK_OUT, HIGH);
  publish("CONNECTED");
  state = 0;
  for (int j = CIRCLE_START_LED; j < CIRCLE_START_LED + ledsPerStrip; j++){
        leds.setPixel(j, 0,0,0);
      }
  leds.show();
}

void onOverride() {
  digitalWrite(MAGLOCK_OUT, LOW);
  state = 1;
  for (int j = CIRCLE_START_LED; j < CIRCLE_START_LED + ledsPerStrip; j++){
        leds.setPixel(j, 0,0,255);
      }
  leds.show();
}
