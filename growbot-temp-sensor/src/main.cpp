#define GB_NODE_TYPE "growbot-temp"

// #define MDNS_NAME "growbot-inside-intake-temp"
// #define LOG_UDP_PORT 44448
//these are defined in the platformio.ini file for each env
//#define MDNS_NAME "growbot-outside-intake-temp" 
//#define LOG_UDP_PORT 44449
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <EzEsp.h>
#include <WebSocketsServer.h>
#include "SparkFun_SHTC3.h"
#include <WSMessenger.h>
#include <TempSensorMsgs.h>

SHTC3 tempSensor;
unsigned long lastTick = 0;
//WebSocketsServer wsServer(8118);
MessengerServer server(WiFi.macAddress(), 8118);

bool sensorOk = false;
bool initSensor() {
  Wire.begin(0,2);
  Wire.setClock(800000);
  int err = tempSensor.begin(Wire);
  if (err != SHTC3_Status_Nominal) {
   // dbg.printf("SHTC3 didn't init: %d\n", err);
  } else {
    err = tempSensor.setMode(SHTC3_CMD_CSE_RHF_NPM);
    if (err != SHTC3_Status_Nominal) {
    //  dbg.printf("SHTC3 didn't setMode: %d\n", err);
    } else {
      err = tempSensor.wake(true);
      if (err != SHTC3_Status_Nominal) {
     //   dbg.printf("SHTC3 didn't wake: %d\n", err);
      } else {
        sensorOk = true;
        return true;
      }
    }
  }
  return false;
}

void setup() {
  Serial.setDebugOutput(true);
  Serial.begin(115200);     
  ezEspSetup(MDNS_NAME, "MaxNet", "88888888");
  initSensor();
}

void loop() {
  ezEspLoop();
  server.handle();
  if (millis() - lastTick > 5000) {
    dbg.printf("device %s\n", MDNS_NAME);
    if (!sensorOk) {
      dbg.println("sensor not initted");
      if (!initSensor()) {
        dbg.println("sensor failed init");
      } else {
        dbg.println("sensor initted!");
      }      
    }
    float rh = NAN;
    float tempc = NAN;
    if (sensorOk) {
      dbg.println("taking reading");
      int err = tempSensor.update();
      if (err != SHTC3_Status_Nominal) {
        dbg.printf("update failed: %d\n", err);
      } else {
        
        if (!tempSensor.passRHcrc) {
          dbg.println("RH isn't valid crc");
        } else {
          rh = tempSensor.toPercent();
          dbg.printf("RH is %f\n", rh);
        }
        if (!tempSensor.passTcrc) {
          dbg.println("temp isn't valid crc");
        } else {
          tempc = tempSensor.toDegC();
          dbg.printf("TempC is %f\n", tempc);
        }
      }
    }
    TempSensorStatusMsg status(WiFi.macAddress(), tempc, rh);
    server.broadcast(status);

    lastTick = millis();
  }
   


}