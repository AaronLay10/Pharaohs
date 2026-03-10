#include <TeensyID.h>

//Code for Final Obelisk Puzzle

uint8_t mac[6];

#define MAGLOCK             12

//MQTT Setup
#define MQTTDELAY 200
long previousMillisMqtt = 0;

#define SENSOR_0            2
#define SENSOR_1            3
#define SENSOR_2            4
#define SENSOR_3            5
#define SENSOR_4            6
#define SENSOR_5            7
#define SENSOR_6            8
#define SENSOR_7            9
#define SENSOR_8            10
#define SENSOR_ANKH         11

short sensorBit = 1;
bool ankh = 1;
int state = 1;
char publishDetail[50] = {0};


void setup() {
  // put your setup code here, to run once:

  pinMode(SENSOR_0, INPUT_PULLUP);
  pinMode(SENSOR_1, INPUT_PULLUP);
  pinMode(SENSOR_2, INPUT_PULLUP);
  pinMode(SENSOR_3, INPUT_PULLUP);
  pinMode(SENSOR_4, INPUT_PULLUP);
  pinMode(SENSOR_5, INPUT_PULLUP);
  pinMode(SENSOR_6, INPUT_PULLUP);
  pinMode(SENSOR_7, INPUT_PULLUP);
  pinMode(SENSOR_8, INPUT_PULLUP);
  pinMode(SENSOR_ANKH, INPUT_PULLUP);
  pinMode(MAGLOCK, OUTPUT);


  digitalWrite(MAGLOCK, HIGH);

  teensyMAC(mac);
  Serial.printf("String MAC Address: %s\n", teensyMAC());

  //Setup the ethernet network connection
 ethernetSetup();
  //Setup the MQTT service
 mqttSetup();
}

void loop() {

  sprintf(publishDetail, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",digitalRead(SENSOR_1),
                                                                             digitalRead(SENSOR_2),
                                                                             digitalRead(SENSOR_3),
                                                                             digitalRead(SENSOR_4),
                                                                             digitalRead(SENSOR_5),
                                                                             digitalRead(SENSOR_6),
                                                                             digitalRead(SENSOR_7),
                                                                             digitalRead(SENSOR_8),
                                                                             digitalRead(SENSOR_0),
                                                                             digitalRead(SENSOR_ANKH));

 unsigned long currentMillisMqtt = millis();
 if(currentMillisMqtt - previousMillisMqtt > MQTTDELAY){
    previousMillisMqtt = currentMillisMqtt;
    Serial.println(publishDetail);
    publish(publishDetail);
    mqttLoop();
 }
}

void onSolve() {
}

void onReset() {
  publish("CONNECTED");
  digitalWrite(MAGLOCK, HIGH);
}

void onOverride() {
}

void onOverrideTwo() {
}

void onSolveTwo() {
}

void onHFDropPanel() {
  digitalWrite(MAGLOCK, LOW);
}
