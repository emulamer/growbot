#define LOG_UDP_PORT 44447
#define MDNS_NAME "growbot-switcheroo"
#define GB_NODE_TYPE "growbot-switcheroo"
#define GB_NODE_ID MDNS_NAME
#include <EzEsp.h>
#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <WSMessenger.h>

const char* WIFI_SSID = "MaxNet";
const char* WIFI_PASSWORD = "88888888";

UdpMessengerServer server(45678);
unsigned long currentTime = millis();
unsigned long previousTime = 0; 
const long timeoutTime = 2000;
bool doBroadcast = false;

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

void onSetPorts(MessageWrapper& mw) { 
  GbResultMsg resp;
  dbg.println("Got set ports msg");

  SwitcherooSetPortsMsg* msg = (SwitcherooSetPortsMsg*)mw.message;
  bool hasOne = false;
  bool changed = false;
  for (int i = 0; i < NUM_PORTS; i++) {
    if (msg->hasPort(i)) {
      hasOne = true;
      bool newPortStatus = msg->getPort(i);
      if (portStatus[i] != newPortStatus) {
        portStatus[i] = newPortStatus;
        changed = true;
      }      
    }
  }
  if (!hasOne) {
    resp.setUnsuccess("No port statuses sent");
  } else {
    resp.setSuccess();
    if (changed) {
      pendingSave = true;
      setPorts();
      doBroadcast = true;
    }
  }
  mw.reply(resp);
}


void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  for (int i = 0; i < NUM_PORTS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
  }

  loadState();
  setPorts();
  ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD);
  
  server.onMessage(NAMEOF(SwitcherooSetPortsMsg), onSetPorts);
  server.init();
}
void broadcastState() {
  SwitcherooStatusMsg msg(portStatus);
  server.broadcast(msg);
  dbg.dprintln("broadcast status");
}
unsigned long lastTick = 0;
void loop() {
  ezEspLoop();
  server.handle();
  if (millis() - lastTick > 10000 || doBroadcast) {
    broadcastState();
    lastTick = millis();
    doBroadcast = false;
  }
  if (pendingSave) {
    saveState();
    pendingSave = false;
  }
}