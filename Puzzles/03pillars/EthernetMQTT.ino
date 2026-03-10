//Arduino Ethernet MQTT

const IPAddress mqttServerIP(192, 168, 0, 200);
// Unique name of this device, used as client ID and Topic Name on MQTT
const char* deviceID = "PillarFour";

// Global Variables 
// Create an instance of the Ethernet client
EthernetClient ethernetClient;
// Create an instance of the MQTT client based on the ethernet client
PubSubClient MQTTclient(ethernetClient);
// The time (from millis()) at which last message was published
long lastMsgTime = 0;
// A buffer to hold messages to be sent/have been received
char msg[64];
// The topic in which to publish a message
char topic[32];
// Counter for number of heartbeat pulses sent
int pulseCount = 0;

// Callback function each time a message is published in any of
// the topics to which this client is subscribed
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  // The message "payload" passed to this function is a byte*
  // Let's first copy its contents to the msg char[] array
  memcpy(msg, payload, length);
  // Add a NULL terminator to the message to make it a correct string
  msg[length] = '\0';

  // Debug
  Serial.print("Message received in topic [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // Act upon the message received

  if(strcmp(msg, "solveOne") == 0) {
    onSolveOne();
  }
  else if(strcmp(msg, "resetOne") == 0) {
    onResetOne();
  }
  else if(strcmp(msg, "overrideOne") == 0) {
    onOverrideOne();
  }
  else if(strcmp(msg, "solveTwo") == 0) {
    onSolveTwo();
  }
  else if(strcmp(msg, "resetTwo") == 0) {
    onResetTwo();
  }
  else if(strcmp(msg, "overrideTwo") == 0) {
    onOverrideTwo();
  }
  else if(strcmp(msg, "startPillars") == 0) {
    startPillars = true;
  }
}

void ethernetSetup() {

  if(!Serial) {
    // Start the serial connection
    Serial.begin(9600);
  }
  
  // start the Ethernet connection:

  Serial.println("Initialize Ethernet:");

  Ethernet.begin(mac);
 
  
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
}

void mqttSetup() {
  // Define some settings for the MQTT client
  MQTTclient.setServer(mqttServerIP, 1883);
  MQTTclient.setCallback(mqttCallback);
  
}

void mqttLoop() {
  // Ensure there's a connection to the MQTT server
  while (!MQTTclient.connected()) {

    // Debug info
    Serial.print("Attempting to connect to MQTT broker at ");
    Serial.println(mqttServerIP);

    // Attempt to connect
    if (MQTTclient.connect(deviceID)) {
    
      // Debug info
      Serial.println("Connected to MQTT broker");
      
      // Once connected, publish an announcement to the ToHost/#deviceID# topic

      snprintf(topic, 32, "ToHost/PillarFour", deviceID);
      snprintf(msg, 64, "CONNECTED", deviceID);
      MQTTclient.publish(topic, msg);
      
      // Subscribe to topics meant for this device
      snprintf(topic, 32, "ToDevice/PillarFour", deviceID);
      
      MQTTclient.subscribe(topic);
      
      // Subscribe to topics meant for all devices
      MQTTclient.subscribe("ToDevice/All");
    }
    else {
      // Debug info why connection could not be made
      Serial.print("Connection to MQTT server failed, rc=");
      Serial.print(MQTTclient.state());
      Serial.println(" trying again in 5 seconds");
      
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
  // Call the main loop to check for and publish messages
  MQTTclient.loop();
}


void publish(char* message){

  snprintf(topic, 32, "ToHost/PillarFour", deviceID);
  MQTTclient.publish(topic, message);
}
