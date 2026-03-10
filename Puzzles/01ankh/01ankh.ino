#include <TeensyID.h>
#define SENSOR_0    2

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;
char publishDetail[10] = {0};

int state = 0;
int sensor = 2;

uint8_t mac[6];

void setup() {
  Serial.begin(9600);

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());
 
  pinMode(SENSOR_0, INPUT_PULLUP);
  pinMode(13, OUTPUT);

  //Setup the ethernet network connection
 //ethernetSetup();
  //Setup the MQTT service
 //mqttSetup();
 //delay(2000);
}

void loop() {  
  unsigned long currentMillisMqtt = millis();
  if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    Serial.print("Sensor = ");
    Serial.println(publishDetail);

    sprintf(publishDetail, "%02X",digitalRead(SENSOR_0));

    if(digitalRead(SENSOR_0) == 0) {
      digitalWrite(13,HIGH);
    } else {digitalWrite(13,LOW);}
  
    publish(publishDetail);
    //mqttLoop();
 }
}

void onReset() {
    publish("CONNECTED");
    mqttLoop();
}
