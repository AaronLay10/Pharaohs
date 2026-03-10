#include <TeensyID.h>

#include <FastLED.h>

uint8_t mac[6];

#define MAGLOCK           11
#define PILLAR_MAGLOCK    10
#define ANKH_MAGLOCK      9
#define POWER_LED_PIN     13

int state = 0;

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;

#define LED_STRIP    8
#define NUM_LEDS_PER_STRIP 16
#define NUM_STRIPS 1
#define NUM_LEDS   NUM_LEDS_PER_STRIP  
CRGB leds[NUM_LEDS_PER_STRIP * NUM_STRIPS];
#define BRIGHTNESS_VALUE 255

#define SENSOR_1 1
#define SENSOR_2 2
#define SENSOR_3 3
#define SENSOR_4 4
#define SENSOR_5 5
#define SENSOR_6 6
#define SENSOR_7 7

String sensorDetail;
char publishDetail[50] = {0};
int ledState = 0;

byte sensorBit = 127; //Seven 1's in Binary is 127


void setup() {
  Serial.begin(9600);
  //sensor setup
  pinMode(SENSOR_1, INPUT_PULLUP);
  pinMode(SENSOR_2, INPUT_PULLUP);
  pinMode(SENSOR_3, INPUT_PULLUP);
  pinMode(SENSOR_4, INPUT_PULLUP);
  pinMode(SENSOR_5, INPUT_PULLUP);
  pinMode(SENSOR_6, INPUT_PULLUP);
  pinMode(SENSOR_7, INPUT_PULLUP);

  //Maglock setup
  pinMode(MAGLOCK, OUTPUT);
  pinMode(PILLAR_MAGLOCK, OUTPUT);
  pinMode(ANKH_MAGLOCK, OUTPUT);
  pinMode(POWER_LED_PIN, OUTPUT);

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  delay(1000);
  // Teensy 4.0 Stuff ==============     
  LEDS.addLeds<NUM_STRIPS, WS2811, LED_STRIP, RGB>(leds, NUM_LEDS_PER_STRIP);
  LEDS.setBrightness(BRIGHTNESS_VALUE);

  digitalWrite(MAGLOCK, HIGH);
  digitalWrite(PILLAR_MAGLOCK, HIGH);
  digitalWrite(ANKH_MAGLOCK, HIGH);
  digitalWrite(POWER_LED_PIN, HIGH);
 //Setup the ethernet network connection
 ethernetSetup();
  //Setup the MQTT service
 mqttSetup();
}

void loop() {
  
 unsigned long currentMillisMqtt = millis();
 if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(SENSOR_1),digitalRead(SENSOR_2),digitalRead(SENSOR_3),digitalRead(SENSOR_4),digitalRead(SENSOR_5),digitalRead(SENSOR_6),digitalRead(SENSOR_7));
    publish(publishDetail);
    Serial.println(publishDetail);
    mqttLoop();
 }

  if(ledState == 1) {
  fill_solid(leds, NUM_LEDS, CRGB::Red);
    LEDS.show();
  }
  if(ledState == 0) {
   fill_solid(leds, NUM_LEDS, CRGB::Black);
   LEDS.show(); 
  }
}

void onSolve(){
  digitalWrite(MAGLOCK, LOW);
}
  
void onOverride() {
    digitalWrite(MAGLOCK, LOW);

  }

void onReset() {
      digitalWrite(MAGLOCK, HIGH);
      publish("CONNECTED");
  }

void onPillarsSolved() {
    digitalWrite(PILLAR_MAGLOCK, LOW);
    ledState = 1;
  }

void onPillarsReset() {
    ledState = 0;
    digitalWrite(PILLAR_MAGLOCK, HIGH);
}

void onAnkhSolved() {
    digitalWrite(ANKH_MAGLOCK, LOW);
  }

void onAnkhReset() {
    digitalWrite(ANKH_MAGLOCK, HIGH);
}
