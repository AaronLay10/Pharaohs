#include "ParagonMQTT.h"

// ---------------------------------------------------------------------------
// Library‑internal globals
// ---------------------------------------------------------------------------
const IPAddress mqttServerIP(192, 168, 20, 8);
const int portNumber = 33002;
const int MQTTdelay = 500;
uint8_t mac[6] = {0};

#if defined(ESP32)
NETWORK_CLIENT networkClient;
#else
NETWORK_CLIENT networkClient;
#endif

PubSubClient MQTTclient(networkClient);
unsigned long previousMillisMQTT = 0;
char publishDetail[128] = "";
std::map<String, ActionHandler> actionMap;

// ---------------------------------------------------------------------------
// Helper – print an IPAddress without relying on toString()
// ---------------------------------------------------------------------------
static void printIPAddress(const IPAddress &ip)
{
  Serial.print(ip[0]);
  Serial.print('.');
  Serial.print(ip[1]);
  Serial.print('.');
  Serial.print(ip[2]);
  Serial.print('.');
  Serial.println(ip[3]);
}

// ---------------------------------------------------------------------------
// Network bring‑up (Ethernet or Wi‑Fi)
// ---------------------------------------------------------------------------
void networkSetup()
{
  if (!Serial)
    Serial.begin(115200);

#if defined(ESP32)
  Serial.printf("Connecting to Wi‑Fi \"%s\" …", wifiSSID);
  WiFi.begin(wifiSSID, wifiPASS);

  int attempts = 0;
  const int maxAttempts = 20; // 10 seconds timeout
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts)
  {
    delay(500);
    Serial.print('.');
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("\nWi‑Fi connected, IP=");
    printIPAddress(WiFi.localIP());
  }
  else
  {
    Serial.println("\nWi‑Fi connection failed!");
    return;
  }

#else // Teensy 4.1 + NativeEthernet
  teensyMAC(mac);

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("MAC=");
  Serial.println(macStr);

  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("Ethernet configuration failed!");
    return;
  }

  // Give Ethernet time to initialize
  delay(1000);

  Serial.print("IP=");
  printIPAddress(Ethernet.localIP());
#endif
}

// ---------------------------------------------------------------------------
// MQTT client setup
// ---------------------------------------------------------------------------
void mqttSetup()
{
  MQTTclient.setServer(mqttServerIP, portNumber);
  MQTTclient.setCallback(mqttCallback);
}

// ---------------------------------------------------------------------------
// Keep MQTT connection alive & (re)subscribe as needed
// ---------------------------------------------------------------------------
void mqttBroker()
{
  if (!MQTTclient.connected())
  {
    Serial.print("MQTT reconnect … ");
    if (MQTTclient.connect(deviceID))
    {
      Serial.println("OK");

      char topic[64];
      snprintf(topic, sizeof(topic), "ToHost/%s", deviceID);
      if (!MQTTclient.publish(topic, "CONNECTED"))
      {
        Serial.println("Failed to publish CONNECTED message");
      }

      snprintf(topic, sizeof(topic), "ToDevice/%s", deviceID);
      if (!MQTTclient.subscribe(topic))
      {
        Serial.printf("Failed to subscribe to %s\n", topic);
      }
      if (!MQTTclient.subscribe("ToDevice/All"))
      {
        Serial.println("Failed to subscribe to ToDevice/All");
      }
    }
    else
    {
      Serial.printf("fail (rc=%d)\n", MQTTclient.state());
    }
  }
  MQTTclient.loop();
}

// ---------------------------------------------------------------------------
void publish(const char *message)
{
  char topic[64];
  snprintf(topic, sizeof(topic), "ToHost/%s", deviceID);
  if (!MQTTclient.publish(topic, message))
  {
    Serial.printf("Failed to publish message: %s\n", message);
  }
}

// ---------------------------------------------------------------------------
void registerAction(const char *action, ActionHandler handler)
{
  actionMap[String(action)] = handler;
}

// ---------------------------------------------------------------------------
void sendDataMQTT()
{
  mqttBroker(); // keep connection alive

  const unsigned long now = millis();
  if (now - previousMillisMQTT < static_cast<unsigned long>(MQTTdelay))
    return;
  previousMillisMQTT = now;

  if (publishDetail[0] != '\0') // send only if buffer non‑empty
  {
    publish(publishDetail);
    Serial.println(publishDetail);
    publishDetail[0] = '\0'; // clear buffer after send
  }
}

// ---------------------------------------------------------------------------
void sendImmediateMQTT(const char *message)
{
  mqttBroker(); // ensure connection is alive

  if (message && message[0] != '\0') // send only if message is non‑empty
  {
    publish(message);
    Serial.print("IMMEDIATE: ");
    Serial.println(message);
  }
}

// ---------------------------------------------------------------------------
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  /*------------------------------------------------------------------
   * 1.  Copy the MQTT payload into a NUL‑terminated buffer
   *     (length is always ≤ MQTTclient.getBufferSize()).
   *-----------------------------------------------------------------*/

  // Add safety check for reasonable message length
  const unsigned int MAX_MSG_SIZE = 512;
  if (length > MAX_MSG_SIZE)
  {
    Serial.printf("MQTT message too large (%u bytes), truncating to %u\n", length, MAX_MSG_SIZE);
    length = MAX_MSG_SIZE;
  }

  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("MQTT [%s] %s\n", topic, msg);

  /*------------------------------------------------------------------
   * 2.  Split only ONCE: find the first space *or* comma.
   *-----------------------------------------------------------------*/
  char *sep = strpbrk(msg, " ,"); // first  ' '  or  ','
  char *action = msg;             // start of the buffer
  char *value = nullptr;

  if (sep)
  {
    *sep = '\0';     // terminate the action string
    value = sep + 1; // everything after the separator
    while (*value == ' ' || *value == ',')
      ++value; // trim leading delims
  }

  if (*action == '\0')
    return; // empty line? bail out

  /*------------------------------------------------------------------
   * 3.  Dispatch
   *-----------------------------------------------------------------*/
  auto it = actionMap.find(String(action));
  if (it != actionMap.end())
    it->second(value ? value : ""); // pass full remainder
  else
    Serial.println(F("Unknown action"));
}

// ---------------------------------------------------------------------------
void getTopic(char *buffer, size_t bufferSize, const char *baseTopic, const char *device)
{
  if (buffer && bufferSize > 0)
  {
    snprintf(buffer, bufferSize, "%s/%s", baseTopic, device);
  }
}
