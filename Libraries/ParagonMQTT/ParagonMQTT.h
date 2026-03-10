/*
 *  PARAGON ESCAPE GAMES – ParagonMQTT.h
 *  Supports Teensy 4.1 (Ethernet) and ESP32 (Wi‑Fi)
 *  Last updated 2025‑04‑21
 */
#ifndef PARAGONMQTT_H
#define PARAGONMQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <map> // for std::map

#if defined(ESP32)
#include <WiFi.h>
#define NETWORK_CLIENT WiFiClient
#else
#include <NativeEthernet.h>
#include <TeensyID.h>
#define NETWORK_CLIENT EthernetClient
#endif

// ---------------------------------------------------------------------------
// Type alias for an action callback (text payload → void)
// ---------------------------------------------------------------------------
using ActionHandler = void (*)(const char *value);

// ---------------------------------------------------------------------------
// Global symbols provided by the .cpp (extern so one definition lives there)
// ---------------------------------------------------------------------------

extern const IPAddress mqttServerIP;
extern const int portNumber;
extern const int MQTTdelay;
extern uint8_t mac[6];

// NOTE:  define  `const char* deviceID = "YourDevice";`  in **your sketch**
extern const char *deviceID;

#if defined(ESP32)
// NOTE:  define  `const char* wifiSSID = "YourSSID";`  and  `const char* wifiPASS = "YourPassword";`  in **your sketch**
extern const char *wifiSSID;
extern const char *wifiPASS;
#endif

extern NETWORK_CLIENT networkClient;
extern PubSubClient MQTTclient;

extern unsigned long previousMillisMQTT;
extern char publishDetail[128];
extern std::map<String, ActionHandler> actionMap;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void networkSetup();
void mqttSetup();
void mqttBroker();
void publish(const char *message);
void sendDataMQTT();
void sendImmediateMQTT(const char *message);
void registerAction(const char *action, ActionHandler handler);
void getTopic(char *buffer, size_t bufferSize, const char *baseTopic, const char *deviceID);
void mqttCallback(char *topic, byte *payload, unsigned int length);

#endif // PARAGONMQTT_H
