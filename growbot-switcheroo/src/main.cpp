#define LOG_UDP_PORT 44447
#include <GrowbotCommon.h>
#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESPRandom.h>

//http server code from https://randomnerdtutorials.com/esp32-web-server-arduino-ide/

const char* WIFI_SSID = "MaxNet";
const char* WIFI_PASSWORD = "88888888";
#define MDNS_NAME "growbot-switcheroo"
#define WEBSOCKET_PORT 8118

//WiFiServer server(80);
WebSocketsServer webSocket(WEBSOCKET_PORT); 
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;

#define NUM_PORTS 6
String header;
bool pendingSave = false;
bool portStatus[NUM_PORTS] = { false, false, false, false, false, false };
int relayPins[NUM_PORTS] = {18, 15, 19, 16, 21, 17};
void broadcastState();
int digitalReadOutputPin(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  if (port == NOT_A_PIN) 
    return LOW;

  return (*portOutputRegister(port) & bit) ? HIGH : LOW;
}

void setPorts() {
  for (int i = 0; i < NUM_PORTS; i++) {
    digitalWrite(relayPins[i], portStatus[i]?HIGH:LOW);
  }
}

bool byte2portStatus(byte b) {
  bool change = false;
  for (int i = 0; i < NUM_PORTS; i++) {
    bool on = (((1<<i) & b) != 0);
    if (portStatus[i] != on) {
      change = true;
    }    
    portStatus[i] = on;
  }
  return change;
}

byte portStatus2Byte() {
  byte b = 0;
  for (int i = 0; i < NUM_PORTS; i++) {
    b = b | (1<<i);
  }
  return b;
}

void saveState() {
  byte b = portStatus2Byte();
  EEPROM.write(0, b);
  EEPROM.commit();
}

void loadState() {
  byte b = EEPROM.read(0);
  byte2portStatus(b);
  setPorts();
}

void makeStatus(StaticJsonDocument<512> &doc) {
  //yeah, should be dynamic base on NUM_PORTS so sue me
  doc["ports"]["port0"] = portStatus[0];
  doc["ports"]["port1"] = portStatus[1];
  doc["ports"]["port2"] = portStatus[2];
  doc["ports"]["port3"] = portStatus[3];
  doc["ports"]["port4"] = portStatus[4];
  doc["ports"]["port5"] = portStatus[5];
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) { 
  
  if (type == WStype_CONNECTED) {
    dbg.println("got a websocket connection");
    return;
  }
  if (type == WStype_DISCONNECTED) {
    dbg.println("a websocket disconnected");
    return;
  }
  if (type != WStype_TEXT) {
    dbg.println("got a non-text websocket message, ignoring it");
    return;
  }
  if (length > 500) {
    dbg.printf("got too big a websocket message, it was %d bytes\n", length);
  }
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, payload, length);
  if (err) {
    dbg.printf("Error deserializing websocket message: %s, data is %s\n", err.c_str(), (char*) payload);
    return;
  }
  const char* cmd = doc["cmd"];
  const char* msgid = doc["msgid"];

  uint8_t uuid_array[16];
  ESPRandom::uuid4(uuid_array);
  doc["msgid"] = ESPRandom::uuidToString(uuid_array);
  if (msgid != NULL) {
    doc["replyto"] = msgid;
  }
  doc["event"] = "cmdresult";
  bool doBroadcast = false;
  if (strcmp(cmd, "get") == 0) {
    makeStatus(doc);
  } else if (strcmp(cmd, "setports") == 0) {
    if (!doc.containsKey("ports")) {
      doc["success"] = false;
      doc["error"] = "ports must be specified";
    } else {
      bool hasOne = false;
      bool hasChanges = false;
      //should be dynamic on NUM_PORTS
      if (doc["ports"].containsKey("port0")) {
        hasOne = true;
        bool on = doc["ports"]["port0"].as<bool>();
        if (on != portStatus[0]) {
          portStatus[0] = on;
          hasChanges = true;
        }
      }
      if (doc["ports"].containsKey("port1")) {
        hasOne = true;
        bool on = doc["ports"]["port1"].as<bool>();
        if (on != portStatus[1]) {
          portStatus[1] = on;
          hasChanges = true;
        }
      }
      if (doc["ports"].containsKey("port2")) {
        hasOne = true;
        bool on = doc["ports"]["port2"].as<bool>();
        if (on != portStatus[2]) {
          portStatus[2] = on;
          hasChanges = true;
        }
      }
      if (doc["ports"].containsKey("port3")) {
        hasOne = true;
        bool on = doc["ports"]["port3"].as<bool>();
        if (on != portStatus[3]) {
          portStatus[3] = on;
          hasChanges = true;
        }
      }
      if (doc["ports"].containsKey("port4")) {
        hasOne = true;
        bool on = doc["ports"]["port4"].as<bool>();
        if (on != portStatus[4]) {
          portStatus[4] = on;
          hasChanges = true;
        }
      }
      if (doc["ports"].containsKey("port5")) {
        hasOne = true;
        bool on = doc["ports"]["port5"].as<bool>();
        if (on != portStatus[5]) {
          portStatus[5] = on;
          hasChanges = true;
        }
      } 
      if (!hasOne) {
        doc["success"] = false;
        doc["error"] = "no port values were specified";
      } else {
        if (hasChanges) {
          dbg.println("Websocket command to update ports, there were changes");
          setPorts();
          pendingSave = true;
          doBroadcast = true;
        } else {
          dbg.println("Websocket command to update ports, but nothing changed");
        }
        makeStatus(doc);
        doc["success"] = true;
      }
    }
  } 
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.println("Failed to serialize reply json");
  } else {
    webSocket.sendTXT(num, jsonStr);
  }
  if (doBroadcast) {
    broadcastState();
  }
}


void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);

  for (int i = 0; i < NUM_PORTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  loadState();
  setPorts();
  growbotCommonSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD);
  
  dbg.println("Starting websocket...");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  dbg.println("Done with startup");

}
void broadcastState() {
  StaticJsonDocument<512> doc;
  doc["event"] = "status";
  makeStatus(doc);
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.eprintln("Failed to serialize broadcast json");
    return;
  }
  webSocket.broadcastTXT(jsonStr);
  dbg.dprintln("broadcast status");
}
unsigned long lastTick = 0;
void loop() {
  growbotCommonLoop();
  webSocket.loop();
  if (millis() - lastTick > 10000) {
    broadcastState();
    lastTick = millis();
  }
  if (pendingSave) {
    saveState();
    pendingSave = false;
  }
}