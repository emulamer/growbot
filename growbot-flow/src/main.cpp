#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "DebugUtils.h"
#include <WebSocketsServer.h>
#include "FlowMeter.h"
#include <ESP32AnalogRead.h>
#include "WaterLevel.h"
#include "SolenoidValve.h"
#include <ArduinoJson.h>
#include <ESPRandom.h>
#include "FlowOperation.h"

#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"
#define MDNS_NAME "growbot-flow"
#define WEBSOCKET_PORT 8118


//const byte PORTPINS[NUM_PORTS] = {14, 32, 33, 25, 26, 27};


WaterLevel waterLevel(35, 34, 500);
FlowMeter inFlowMeter(5, 2280);
FlowMeter outFlowMeter(21, 2280); //2280 is probably wrong
SolenoidValve inValve(26, 500, 20);//both valve pulse time and hold values are probably wrong
SolenoidValve outValve(27, 500, 20);


FlowOp* currentFlowOp = NULL;



WebSocketsServer webSocket(WEBSOCKET_PORT);   


void wifi_event(system_event_t *sys_event, wifi_prov_event_t *prov_event) {
  if (WiFi.status() == wl_status_t::WL_CONNECTED) {
    dbg.wifiIsReady();
    dbg.printf("Got IP address\n");
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) { 
  dbg.println("got a websocket msg");
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
  const char* field = doc["field"];
  const char* op = doc["op"];
  float value = NAN;
  if (doc.containsKey("value")) {
    value = doc["value"].as<float>();
  }
  doc.clear();
  uint8_t uuid_array[16];
  ESPRandom::uuid4(uuid_array);
  doc["msgid"] = ESPRandom::uuidToString(uuid_array);
  if (msgid != NULL) {
    doc["replyto"] = msgid;
  }
  if (cmd == NULL) {
    doc["success"] = false;
    doc["error"] = "there was no cmd in ws message";
    dbg.printf("there was no cmd in ws message\n");
  } else if (strcmp(cmd, "abortop") == 0) {
    if (currentFlowOp == NULL) {
      doc["success"] = false;
      doc["error"] = "no op in progress";
    } else if (currentFlowOp->isDone()) {
      doc["success"] = false;
      doc["error"] = "op has already finished";
    } else {
      dbg.println("aborting op");
      currentFlowOp->abort();
      doc["success"] = true;
    }
  } else if (strcmp(cmd, "startop") == 0 ) {
    if (op == NULL) {
      doc["success"] = false;
      doc["error"] = "op is required";
      dbg.printf("no op was provided for startop\n");
    } else if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
      dbg.printf("tried starting an op while one was in progress already\n");
      doc["success"] = false;
      doc["error"] = "another op is already in progress";
    } else {
      FlowOp* newOp = NULL;
      if (strcmp(op, "DrainToPercent") == 0) {
        if (isnan(value) || value < 0 || value >= 100) {
          doc["success"] = false;
          doc["error"] = "invalid value";
        } else {
          newOp = new DrainToPercentOp(&waterLevel, &outValve, value);
          doc["success"] = true;
        }        
      } else if (strcmp(op, "Empty") == 0) {
        newOp = new EmptyOp(&waterLevel, &outValve);
        doc["success"] = true;
      } else if (strcmp(op, "FillToPercent") == 0) {
        if (isnan(value) || value <= 0 || value > 100) {
          doc["success"] = false;
          doc["error"] = "invalid value";
        } else {
          newOp = new FillToPercentOp(&waterLevel, &inValve, value);
          doc["success"] = true;
        }
      } else if (strcmp(op, "Flush") == 0) {
        newOp = new FlushOp(&waterLevel, &inValve, &outValve);
        doc["success"] = true;
      } else if (strcmp(op, "FlushAndFillToPercent") == 0) {
        if (isnan(value) || value <= 0 || value > 100) {
          doc["success"] = false;
          doc["error"] = "invalid value";
        } else {
          newOp = new FlushAndFillToPercentOp(&waterLevel, &inValve, &outValve, value);
          doc["success"] = true;          
        }        
      } else {
        doc["success"] = false;
        doc["error"] = "unknown op type";
        dbg.printf("unknown op type %s\n", op);
      }
      if (newOp != NULL) {
        if (currentFlowOp != NULL) {
          delete currentFlowOp;
          currentFlowOp = NULL;
        }
        currentFlowOp = newOp;
        dbg.printf("Starting new op of type %d\n", currentFlowOp->getType());
        currentFlowOp->start();
      } 
    }
  } else if (strcmp(cmd, "get") == 0) {
    bool addedOne = false;
    if (field == NULL || strcmp(field, "waterLevel") == 0) {
      doc["waterLevel"] = waterLevel.getWaterLevel();  
      addedOne = true;
    }
    if (field == NULL || strcmp(field, "litersIn") == 0) {
      doc["litersIn"] = inFlowMeter.getLitersSinceReset();  
      addedOne = true;
    }
    if (field == NULL || strcmp(field, "litersOut") == 0) {
      doc["litersOut"] = outFlowMeter.getLitersSinceReset();
      addedOne = true;
    }
    if (field == NULL || strcmp(field, "inValveOpen") == 0) {
      doc["inValveOpen"] = inValve.isOn();
      addedOne = true;
    }
    if (field == NULL || strcmp(field, "outValveOpen") == 0) {
      doc["outValveOpen"] = outValve.isOn();
      addedOne = true;
    }
    if (field == NULL || strcmp(field, "currentOperation") == 0) {
      if (currentFlowOp != NULL) {
        switch (currentFlowOp->getType()) {
          case FlowOpType::DrainToPercent:
            doc["currentOperation"]["type"] = "DrainToPercent";
            break;
          case FlowOpType::Empty:
            doc["currentOperation"]["type"] = "Empty";
            break;
          case FlowOpType::FillToPercent:
            doc["currentOperation"]["type"] = "FillToPercent";
            break;
          case FlowOpType::Flush:
            doc["currentOperation"]["type"] = "Flush";
            break;
          case FlowOpType::FlushAndFillToPercent:
            doc["currentOperation"]["type"] = "FlushAndFillToPercent";
            break;
          case FlowOpType::Wait:
            doc["currentOperation"]["type"] = "Wait";
            break;
        }
        doc["currentOperation"]["status"] = currentFlowOp->isFailed()?"failed":(currentFlowOp->isDone()?"done":"running");
      } else {
        doc["currentOperation"]["type"] = "None";
      }
      addedOne = true;
    }
    if (!addedOne) {
      doc["success"] = false;
      doc["error"] = "field is invalid";
    } else {
      doc["success"] = true;
    }
  } else if (strcmp(cmd, "reset") == 0) {
    if (isnan(value) || value <= 0) {
      doc["success"] = false;
      doc["error"] = "invalid value";
    } else {
      if (strcmp(field, "litersIn")) {
        inFlowMeter.resetFrom(value);
        doc["success"] = true;
      } else if (strcmp(field, "litersOut")) {
        outFlowMeter.resetFrom(value);
        doc["success"] = true;
      } else {
        doc["success"] = false;
        doc["error"] = "unknown field";
      }
    }

  } else if (strcmp(cmd, "fillToPercent") == 0) {
    if (isnan(value) || value <= 0) {
      doc["success"] = false;
      doc["error"] = "invalid value";
    } else {


    }
  } else {
    doc["success"] = false;
    doc["error"] = "unknown cmd";
    dbg.printf("unknown cmd %s\n", cmd);
  }
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.println("Failed to serialize reply json");
    return;
  }
  webSocket.sendTXT(num, jsonStr);    
}



void setup() {
  Serial.begin(9600);
  
  dbg.println("Starting Wifi...");
  WiFi.onEvent(wifi_event);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  
  dbg.println("Setting up OTA...");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart([]() {
      dbg.printf("Start updating\n");
      outValve.setOn(false);
      inValve.setOn(false);
    });
    ArduinoOTA.onEnd([]() {
      dbg.println("Update End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      dbg.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      dbg.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) dbg.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) dbg.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) dbg.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) dbg.println("Receive Failed");
      else if (error == OTA_END_ERROR) dbg.println("End Failed");
    });
  ArduinoOTA.begin();
  dbg.println("Starting websocket...");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  dbg.println("Starting mDNS...");
  MDNS.begin(MDNS_NAME);
  dbg.println("Done startup");
}
unsigned long last = millis();
void loop() {
  webSocket.loop();
  ArduinoOTA.handle();
  inValve.update();
  outValve.update();
  waterLevel.update();
  if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
    currentFlowOp->handle();
    if (currentFlowOp->isDone()) {
      //just finished
      
      if (currentFlowOp->isFailed()) {
        //er.. failed
        dbg.println("op just failed");
      } else {
        //nah done good
        dbg.println("op just succeeded");
        
      }
    }
  }
  if (millis() - last > 5000) {
    dbg.printf("avg water level: %f\n", waterLevel.getWaterLevel());
    dbg.printf("In Flow: %f\n", inFlowMeter.getLitersSinceReset());
    dbg.printf("Out Flow: %f\n", outFlowMeter.getLitersSinceReset());
    dbg.printf("wifi RSSI: %d\n", WiFi.RSSI());
    last = millis();
  }
  
}