#include <Adafruit_NeoPixel.h>

#define ACTIVATION_PIN  2
#define LED_STRIP       3

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
  Serial.begin(9600);
  strip.begin();
  strip.show();

  pinMode(ACTIVATION_PIN, INPUT_PULLDOWN);
}

void loop(){
  if(digitalRead(ACTIVATION_PIN) == HIGH){
    Serial.println("Hothor");
    hothorled();
    Serial.println("Toth");
    tothled();
    Serial.println("Osiris");
    osirisled();
    Serial.println("Bost");
    bostled();
    Serial.println("Anubis");
    anubisled();
    Serial.println("Horus");
    horusled();
    Serial.println("Isis");
    isisled();
  }
  if(digitalRead(ACTIVATION_PIN) == LOW){
  Serial.println("Waiting");
  }

}
void osirisled(){
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
