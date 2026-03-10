#include <TeensyID.h>

#define MAGLOCK     9

#define SENSOR_0    2
#define SENSOR_1    3
#define SENSOR_2    4
#define SENSOR_3    5
#define SENSOR_4    6
#define SENSOR_5    7
#define SENSOR_6    8

#define MQTTDELAY 200
long previousMillisMqtt = 0;
int state = 0;

uint8_t mac[6];

String sensorDetail;
char publishDetail[50] = {0};

void setup() {
  Serial.begin(9600);
// put your setup code here, to run once:
  pinMode(MAGLOCK, OUTPUT);
  pinMode(SENSOR_0, INPUT_PULLUP);
  pinMode(SENSOR_1, INPUT_PULLUP);
  pinMode(SENSOR_2, INPUT_PULLUP);
  pinMode(SENSOR_3, INPUT_PULLUP);
  pinMode(SENSOR_4, INPUT_PULLUP);
  pinMode(SENSOR_5, INPUT_PULLUP);
  pinMode(SENSOR_6, INPUT_PULLUP);

  digitalWrite(MAGLOCK, HIGH); //Transistor used

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());


  //Setup the ethernet network connection
 ethernetSetup();
  //Setup the MQTT service
 mqttSetup();
}

void loop() {
  unsigned long currentMillisMqtt = millis();
   if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
      previousMillisMqtt = currentMillisMqtt;
      publish(publishDetail);
      mqttLoop();
    
  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(SENSOR_0),digitalRead(SENSOR_1),digitalRead(SENSOR_2),digitalRead(SENSOR_3),digitalRead(SENSOR_4),digitalRead(SENSOR_5),digitalRead(SENSOR_6));

  Serial.println(publishDetail);
  
   }


}

void onSolve() {
  digitalWrite(MAGLOCK, LOW);
}

void onReset() {
  digitalWrite(MAGLOCK,HIGH);
  publish("CONNECTED");
}

void onOverride () {
  digitalWrite(MAGLOCK, LOW);
}
