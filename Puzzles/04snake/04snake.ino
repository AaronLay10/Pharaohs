#include <TeensyID.h>
#include <FastLED.h>

#define MAGLOCK             14
#define DATA_PIN             4
#define POWER_LED           13
#define NUM_LEDS            12

CRGB leds[NUM_LEDS];

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;

uint8_t mac[6];
long previousMillis = 0;
char publishDetail[50] = {0};

//Array of Sensor Pins for Amulets - Not limited to a specific number - Could add 8 to the brackets
int slots[] = {12, 11, 10, 9, 8, 6, 7, 5}; //: All slot pins
int sensors = 0;

//This is a 2D Array named "ons" that has a non defined number of rows, but has 8 columns, the result of this array is True or False
boolean ons[][8] = {
  {true, false, true, false, true, true, false, true},
  {true, true, false, true, false, true, true, false},
  {true, false, true, true, true, false, true, false},
  {false, true, true, true, true, false, false, true},
  {true, true, true, false, true, false, true, false},
  {false, false, false, true, true, true, true, true},
  {false, true, true, false, false, true, true, true},
  {true, true, false, true, false, true, false, true}
};


void setup() {
  
  //Loop through pins and set them as Inputs with Pull up resisitors
  for (int i = 0; i < 8; i++) {
    pinMode(slots[i], INPUT_PULLUP);
  }

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show(); 

//Set up other Digital Pins
  pinMode(MAGLOCK, OUTPUT);
  pinMode(POWER_LED, OUTPUT);

//Set default state for output pins
  digitalWrite(MAGLOCK, HIGH);
  digitalWrite(POWER_LED, HIGH);

  //> Init Serial
  Serial.begin(115200);
  Serial.println("STARTING");

  //Setup the ethernet network connection
ethernetSetup();
  //Setup the MQTT service
mqttSetup();
}

void loop() {
     
  //Reading sensors into a Binary Byte (8 characters)
  sensors = digitalRead(slots[0]) | digitalRead(slots[1]) <<1 | digitalRead(slots[2]) <<2 | digitalRead(slots[3]) <<3 | digitalRead(slots[4]) <<4 | digitalRead(slots[5]) <<5 | digitalRead(slots[6]) <<6 | digitalRead(slots[7]) <<7;
   
  unsigned long currentMillisMqtt = millis();
  if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(slots[0]),digitalRead(slots[1]),digitalRead(slots[2]),digitalRead(slots[3]),digitalRead(slots[4]),digitalRead(slots[5]),digitalRead(slots[6]),digitalRead(slots[7]));
    Serial.println(publishDetail);
    publish(publishDetail);
    FastLED.show();
    
    mqttLoop();
  }  
}


void onSolve() {
  Serial.println(publishDetail);
  Serial.println("Solved");
  digitalWrite(MAGLOCK, LOW);
  fill_solid(leds, NUM_LEDS, CRGB::Green);
  FastLED.show();
}

void onReset() {
  Serial.println("Reset!!");
  publish("CONNECTED");
  digitalWrite(MAGLOCK, HIGH);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show(); 
}

void onOverride(){
  Serial.println("OVERRIDE!!!");
  digitalWrite(MAGLOCK, LOW);
  fill_solid(leds, NUM_LEDS, CRGB::Green);
  FastLED.show();
}
