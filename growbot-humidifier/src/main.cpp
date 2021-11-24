#define GB_NODE_TYPE "growbot-humidifier"
#define MDNS_NAME "growbot-humidifier"
#define LOG_UDP_PORT 44453
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <EzEsp.h>
#include <WebSocketsServer.h>
#include <WebSocketsClient.h>
#include <WSMessenger.h>
#include <HumidifierMsgs.h>
#include <EEPROM.h>

#define HUMIDIFIER_PIN 1
#define SOCKET_RECONNECT_PERIOD 10000
unsigned long lastTick = 0;
MessengerServer server(WiFi.macAddress(), 8118);
WebSocketsClient webSocket;
int currentMode = 0;
float onHumidityPercent = NAN;
float offHumidityPercent = NAN;
float currentHumidityPercent = NAN;
bool isOn = false;
String targetSensorName = "";

//returns true if humidifier state changed
bool setHumidifier() {
    bool wasOn = isOn;
    if (currentMode == HUMIDIFIER_MODE_ON) {
      isOn = true;
    } else if (currentMode == HUMIDIFIER_MODE_AUTO) {
      if (isnan(currentHumidityPercent)) {
       // dbg.printf("No currentHumidityPercent reading!\n");
        isOn = false;
      } else {
        if (currentHumidityPercent <= onHumidityPercent) {
          isOn = true;
        } else if (currentHumidityPercent >= offHumidityPercent) {
          isOn = false;
        }
      }
    } else {
      isOn = false;
    }
    digitalWrite(HUMIDIFIER_PIN, isOn);
    if (wasOn != isOn) {
      dbg.printf("ON changed from %d to %d\n", wasOn, isOn);
      return true;
    }
    return false;
}
unsigned long lastConnectAttemp = 0;
String currentSocketHost = "";
void checkSocketClient() {
    if (currentSocketHost.equals(targetSensorName)) {
      return;
    }
    if (webSocket.isConnected()) {
      dbg.printf("Disconnecting websocket...");
      webSocket.disconnect();
    } else if (millis() - lastConnectAttemp < SOCKET_RECONNECT_PERIOD) {
      return;
    }
    lastConnectAttemp = millis();
    if (targetSensorName.length() < 1 || targetSensorName.equals("null")) {
      dbg.printf("target sensor name isn't set!\n");
      return;
    }
    dbg.printf("connecting to target sensor: %s\n", targetSensorName.c_str());
    IPAddress addr;
    if (!Resolver.resolve(targetSensorName, &addr)) {
      dbg.printf("Failed to resolve target sensor name %s!\n", targetSensorName.c_str());
    } else {
      dbg.printf("Resolved %s to %s, connecting...\n", targetSensorName.c_str(), addr.toString().c_str());
      webSocket.begin(addr, 8118);
    }      
}
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            dbg.eprintf("client: websocket disconnected!\n");
            break;
        case WStype_CONNECTED:
            dbg.println("client: websocket connected");
            currentSocketHost = targetSensorName;
            break;
        case WStype_TEXT:
            GbMsg* msg = parseGbMsg((char*)payload, length);
            if (msg == NULL) {
                dbg.wprintln("client: unparseable message!");
                return;
            }
            if (msg->myType().equals(NAMEOF(TempSensorStatusMsg))) {
                TempSensorStatusMsg* m = (TempSensorStatusMsg*)msg;
                currentHumidityPercent = m->humidityPercent();                
                dbg.dprintf("client: got updated humidity: %f\n", currentHumidityPercent);
            } else {
                dbg.dprintf("client: some other msg type we don't care: %s\n", msg->myType().c_str());
            }
            delete msg;
            break;
    }
}
void broadcastStatus() {
  HumidifierStatusMsg status(WiFi.macAddress(), currentMode, onHumidityPercent, offHumidityPercent, targetSensorName, isOn, currentHumidityPercent);
  server.broadcast(status);
}
void setMsg(MessageWrapper& mw) {
  HumidifierSetMsg* msg = (HumidifierSetMsg*)mw.message;
  GbResultMsg repl(WiFi.macAddress());
  int mode = msg->mode();
  String sensorName = msg->targetSensorName();
  float onPct = msg->onHumidityPercent();
  float offPct = msg->offHumidityPercent();
    if (sensorName == NULL) {
      dbg.println("sensor name is null");
    }
    if (targetSensorName == NULL) {
      dbg.println("target sensor name is null");
    }
  dbg.printf("got set msg, mode: %d, on humidity: %f, off humidity %f, sensorName: %s\n", mode, onPct, offPct, sensorName.c_str());
  bool isOk = true;
  if (mode == HUMIDIFIER_MODE_AUTO) {
    if (isnan(onPct)) {
      repl.setUnsuccess("onHumidityPercent is required for auto mode");
      isOk = false;
    } else if (isnan(offPct)) {
      repl.setUnsuccess("offHumidityPercent is required for auto mode");
      isOk = false;
    } else if ((sensorName == NULL || sensorName.length() < 1 || sensorName.equals("null")) && (targetSensorName == NULL || targetSensorName.length() < 1 || targetSensorName.equals("null"))) {
      repl.setUnsuccess("targetSensorName is not set and must be provided for auto mode");
      isOk = false;
    } else {
      onHumidityPercent = onPct;
      offHumidityPercent = offPct;
    }
  }
  
  if (isOk) {
    currentMode = mode;
  
    if (!(sensorName.length() < 1 || sensorName.equals("null"))) {
      //sensor name change, reconfigure stuff
      targetSensorName = sensorName;
      checkSocketClient();
      currentHumidityPercent = NAN;
    }
    EEPROM.put(0, currentMode);
    EEPROM.put(4, onHumidityPercent);
    EEPROM.put(8, offHumidityPercent);
    int len = targetSensorName.length();
    EEPROM.put(32, len);
    const char* str = targetSensorName.c_str();
    for (int i = 0; i < len; i++) {
      EEPROM.write(36 + i, str[i]);
    }
    EEPROM.commit();
    if (setHumidifier()) {
      broadcastStatus();
    }
    repl.setSuccess();
  }  
  dbg.printf("gonna do reply\n");
  mw.reply(repl);
}

void setup() {
  Serial.setDebugOutput(true);
  Serial.begin(115200);
  EEPROM.begin(512);
  //GPIO 3 (RX) swap the pin to a GPIO.
  pinMode(HUMIDIFIER_PIN, FUNCTION_3); 
  ezEspSetup(MDNS_NAME, "MaxNet", "88888888");
  EEPROM.get(0, currentMode);
  EEPROM.get(4, onHumidityPercent);  
  EEPROM.get(8, offHumidityPercent);
  int len = 0;
  EEPROM.get(32, len);
  if (len >= 0 && len < 200) {
    char* str = (char*)malloc(len+1);
    str[len] = '\0';
    for (int i = 0; i < len; i++) {
      str[i] = EEPROM.read(36 + i);
    }
    dbg.printf("read string len %d: %s\n", len, str);
    targetSensorName = String(str);
    free(str);
  } else {
    dbg.printf("target sensor name didn't read good!\n");
    len = 0;
    EEPROM.put(32, len);
    targetSensorName = "";
  }
  dbg.printf("target sensor name: %s\n", targetSensorName.c_str());
  
  server.onMessage(NAMEOF(HumidifierSetMsg), setMsg);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(SOCKET_RECONNECT_PERIOD);
  webSocket.enableHeartbeat(10000, 3000, 2);
}

void loop() {
  ezEspLoop();
  server.handle();
  webSocket.loop();
  if (!webSocket.isConnected()) {
    checkSocketClient();
  }
  setHumidifier();
  if (millis() - lastTick > 5000) {    
    dbg.printf("device %s, free heap: %d\n", MDNS_NAME, ESP.getFreeHeap());
    dbg.printf("isOn: %d, target sensor: %s, currentHumidityPercent: %f, mode: %d, on at: %f, off at: %f, sensor connected: %d\n", isOn, targetSensorName.c_str(), currentHumidityPercent, currentMode, onHumidityPercent, offHumidityPercent, webSocket.isConnected());
    
    broadcastStatus();
    lastTick = millis();
  }
}