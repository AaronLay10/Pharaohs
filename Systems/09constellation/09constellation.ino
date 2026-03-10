#include <TeensyID.h>

#include <Adafruit_NeoPixel.h>

uint8_t mac[6];

#define ACTIVATION_PIN  3
#define LED_STRIP       2

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;
char publishDetail[10] = {0};

long previousMillis = 0;

bool stopAction = false;
bool runStatus = true;



Adafruit_NeoPixel strip = Adafruit_NeoPixel(200, LED_STRIP, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel. Avoid connecting
// on a live circuit... if you must, connect GND first.

//Constelation Arrays (Numbers 1-4 are place holders so that timing is equal between the constelations)

int osiris[] = {1,26,29,33,34,35,37,41,44}; // 8
int bost[] = {59,61,63,65,66,70,71,73,74}; // 9
int anubis[] = {86,89,91,93,97,102,106,109,111}; // 9
int isis[] = {1,115,118,121,124,126,130,133,136}; //8
int horus[] = {1,147,149,150,153,154,156,160,162}; //8
int toth[] = {1,2,3,169,172,174,178,180,182}; //6
int hothor[] = {1,2,3,4,189,191,193,196,199}; //5

int s = 3; //Speed
int b = 255; //Brightness

void setup() {
  Serial.begin(115200);

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());
  
  strip.begin();
  strip.show();

  pinMode(ACTIVATION_PIN, INPUT_PULLDOWN);

  ethernetSetup();
  mqttSetup();
  delay(2000);
}

void loop(){

   unsigned long currentMillisMqtt = millis();
   if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
      previousMillisMqtt = currentMillisMqtt;
      mqttLoop();
  }
}

void runAnimation() {
    stopAction = false;
    Serial.println("Constelation Running");
    Serial.println("One");
    char constNum = "One";
    checkMessages();
    hothorled();
    
    if (stopAction){
      return;
    }
    Serial.println("Two");
    checkMessages();
    tothled();
    
    if (stopAction){
      return;
    }
    Serial.println("Three");
    checkMessages();
    osirisled();
    
    if (stopAction){
      return;
    }
    Serial.println("Four");
    checkMessages();
    bostled();
    
    if (stopAction){
      return;
    }
    Serial.println("Five");
    checkMessages();
    anubisled();
    
    if (stopAction){
      return;
    }
    Serial.println("Six");
    checkMessages();
    horusled();
    
    if (stopAction){
      return;
    }
    Serial.println("Seven");
    checkMessages();
    isisled();
    
    Serial.println("Animation Finished");
    publish("Constellations Finished");
    runStatus = false;
  }
  
void checkMessages() {
  Serial.println("Checking for Messages");
  
  mqttLoop();
}
  

void osirisled(){
  publish("Three");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(osiris[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(osiris[n],i,i,i);
      strip.show();
    }
  }
}

void anubisled(){
  publish("Five");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(anubis[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(anubis[n],i,i,i);
      strip.show();
    }
  }
}

void tothled(){
  publish("Two");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(toth[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(toth[n],i,i,i);
      strip.show();
    }
  }
}

void bostled(){
  publish("Four");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(bost[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(bost[n],i,i,i);
      strip.show();
    }
  }
}

void horusled(){
  publish("Six");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(horus[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(horus[n],i,i,i);
      strip.show();
    }
  }
}

void hothorled(){
  publish("One");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(hothor[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(hothor[n],i,i,i);
      strip.show();
    }
  }
}

void isisled(){
  publish("Seven");
  for (int i = 0; i <= b; i = i + s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(isis[n],i,i,i);
      strip.show();
    } 
  }
  for (int i = b; i >= 0; i = i - s){
    for (int n = 0; n < 9; n++){
      strip.setPixelColor(isis[n],i,i,i);
      strip.show();
    }
  }
}

void OnStop(){
  stopAction = true;
  publish("Stop");
  runStatus = false;
}

void OnStart(){
  Serial.println(runStatus);
  if (runStatus == false){
    Serial.println("Running Animation from command");
    runStatus = true;
    runAnimation();
  } else {
  Serial.println ("runStatus not stopped");
}}
