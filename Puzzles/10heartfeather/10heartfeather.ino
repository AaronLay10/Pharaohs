#include <TeensyID.h>

// Heart and Feather Puzzle Code
/*
 * **********************Program Teensy at 150 MHz*********************************
 */

#include "HX711.h"

uint8_t mac[6];

#define calibration_factor -7050.0 //This value is obtained using the SparkFun_HX711_Calibration sketch

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;


//Variable to mark Program One complete.


//Load Cell Pins
#define CLK   2
#define DAT   3
#define CLK_2 4
#define DAT_2 5
//To Control

//To Maglock Drop Panel from Program 2 Obelisk
#define MAGLOCK_ONE  9
#define MAGLOCK_TWO  10
#define BOXMAGLOCKS 11
#define BOXLEDS 12
#define POWER_LED 13

//Variables
float heartWeight = 0;
float featherWeight = 0;
int heartWeightMax = 0;
int heartWeightMin = 0;
int featherWeightMax = 0;
int featherWeightMin = 0;
char result[30];
char heart[15];
char feather[15];

int state = 1;

HX711 scale;
HX711 scale2;

void setup() {
  Serial.begin(115200);
  Serial.println("side Load Cell Puzzle");
  
  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

   //Setup the ethernet network connection
  ethernetSetup();
  //Setup the MQTT service
  mqttSetup();
  mqttLoop();

  //Set Pin Modes
  pinMode(MAGLOCK_ONE, OUTPUT);
  pinMode(MAGLOCK_TWO, OUTPUT);
  pinMode(BOXMAGLOCKS, OUTPUT);
  pinMode(BOXLEDS, OUTPUT);
  pinMode(POWER_LED, OUTPUT);
    
  digitalWrite(MAGLOCK_ONE, HIGH);
  digitalWrite(MAGLOCK_TWO, HIGH);
  digitalWrite(BOXMAGLOCKS, HIGH);
  digitalWrite(BOXLEDS, LOW);
  digitalWrite(POWER_LED, HIGH);

  

  //Scales
  scale.begin(DAT, CLK); 
  scale.set_scale(calibration_factor);
 
  scale2.begin(DAT_2, CLK_2);
  scale2.set_scale(calibration_factor);
  
  scale.tare();
  delay(500);
  scale2.tare();
  
}

void loop() {
  char buf[30];

 
 
  unsigned long currentMillisMqtt = millis();
 if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    heartWeight = scale.get_units(5);
    featherWeight = scale2.get_units(5);
  
    dtostrf(heartWeight, 6, 2, heart);
    dtostrf(featherWeight, 6, 2, feather);

    const char *first = heart;
    const char *second = ":";
    const char *third = feather;
    strcpy(buf,first);
    strcat(buf,second);
    strcat(buf,third);

    Serial.println(buf);  //prints sent message
  
    publish(buf);
    mqttLoop();
 }
}

void onSolve() {
  state = 3;
}

void onOverride(){
  onSolve();
}

void onReset(){
  Serial.println("Reset!!");
  publish("CONNECTED");
  digitalWrite(MAGLOCK_ONE, HIGH);
  digitalWrite(MAGLOCK_TWO, HIGH);
  digitalWrite(BOXMAGLOCKS, HIGH);
  digitalWrite(BOXLEDS, LOW);
  state = 1;
}

void onDropPanel() {
  Serial.println("In Drop Panel Sequence...");
  digitalWrite(MAGLOCK_ONE, LOW);
  digitalWrite(MAGLOCK_TWO, LOW);
  }

void onBoxOpen() {
  Serial.println("Boxes Opening...");
  digitalWrite(BOXMAGLOCKS, LOW);
  digitalWrite(BOXLEDS, HIGH);
}

void tareWeight(){
  scale.tare();
  delay(500);
  scale2.tare();
}
