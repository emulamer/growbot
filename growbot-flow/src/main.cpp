#define LOG_UDP_PORT 44446
#define GB_NODE_TYPE "growbot-flow"
#include <GrowbotCommon.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
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



WaterLevel waterLevel(35, 34, 500);
FlowMeter inFlowMeter(5, 875);
FlowMeter outFlowMeter(21, 586); 

SolenoidValve inValve(33, 300, 15);//both valve pulse time and hold values are probably wrong
SolenoidValve outValve(27, 300, 15);
// SolenoidValve valve3(26, 500, 20);
// SolenoidValve valve4(14, 500, 20);
// SolenoidValve valve5(32, 500, 20);
// SolenoidValve valve6(25, 500, 20);


//connector side:  33 32  14??? something wrong, 32 always triggers
// other side: 33, 32, 25

FlowOp* currentFlowOp = NULL;



WebSocketsServer webSocket(WEBSOCKET_PORT);   
void makeOp(StaticJsonDocument<512> &doc) {
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
      case FlowOpType::FillToPercentOnce:
        doc["currentOperation"]["type"] = "FillToPercentOnce";
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
    doc["currentOperation"]["opInFlow"] = currentFlowOp->opInFlow;
    doc["currentOperation"]["opOutFlow"] = currentFlowOp->opOutFlow;
    doc["currentOperation"]["opTotalDeltaFlow"] = currentFlowOp->opTotalDeltaFlow;
  } else {
    doc["currentOperation"]["type"] = "None";
  }
}
void broadcastOpComplete() {
  if (currentFlowOp == NULL) {
    return;
  }
  StaticJsonDocument<512> doc;
  doc["event"] = "endop";
  makeOp(doc);
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.println("Failed to serialize reply json");
    return;
  }
  webSocket.broadcastTXT(jsonStr);
}

void abortOp() {
  if (currentFlowOp == NULL || currentFlowOp->isDone()) {
    return;
  }
  currentFlowOp->abort();
  broadcastOpComplete();
}

void broadcastOpStart() {
  if (currentFlowOp == NULL) {
    return;
  }
  StaticJsonDocument<512> doc;
  doc["event"] = "startop";
  makeOp(doc);
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.println("Failed to serialize reply json");
    return;
  }
  webSocket.broadcastTXT(jsonStr);
}
void broadcastStatus() {
  
  StaticJsonDocument<512> doc;
  doc["event"] = "status";
  doc["waterLevel"] = waterLevel.getWaterLevel();  

  doc["litersIn"] = inFlowMeter.getLitersSinceReset();  
  doc["litersOut"] = outFlowMeter.getLitersSinceReset();
  doc["inValveOpen"] = inValve.isOn();
  doc["outValveOpen"] = outValve.isOn();
  makeOp(doc);
  String jsonStr;
  
  if (serializeJson(doc, jsonStr) == 0) {
    dbg.eprintln("Failed to serialize reply json");
    return;
  }
  webSocket.broadcastTXT(jsonStr);
  dbg.dprintln("broadcast status");
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
  doc["event"] = "cmdresult";
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
      abortOp();
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
          newOp = new DrainToPercentOp(&waterLevel, &outValve, &inFlowMeter, &outFlowMeter, value);
          doc["success"] = true;
        }        
      } else if (strcmp(op, "Empty") == 0) {
        newOp = new EmptyOp(&waterLevel, &outValve, &inFlowMeter, &outFlowMeter);
        doc["success"] = true;
      } else if (strcmp(op, "FillToPercent") == 0) {
        if (isnan(value) || value <= 0 || value > 100) {
          doc["success"] = false;
          doc["error"] = "invalid value";
        } else {
          newOp = new FillToPercentEnsured(&waterLevel, &inValve, &inFlowMeter, &outFlowMeter, value);
          doc["success"] = true;
        }
      } else if (strcmp(op, "Flush") == 0) {
        newOp = new FlushOp(&waterLevel, &inValve, &outValve, &inFlowMeter, &outFlowMeter);
        doc["success"] = true;
      } else if (strcmp(op, "FlushAndFillToPercent") == 0) {
        if (isnan(value) || value <= 0 || value > 100) {
          doc["success"] = false;
          doc["error"] = "invalid value";
        } else {
          newOp = new FlushAndFillToPercentOp(&waterLevel, &inValve, &outValve, &inFlowMeter, &outFlowMeter, value);
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
        broadcastOpStart();
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
          case FlowOpType::FillToPercentOnce:
            doc["currentOperation"]["type"] = "FillToPercentOnce";
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
        doc["currentOperation"]["opInFlow"] = currentFlowOp->opInFlow;
        doc["currentOperation"]["opOutFlow"] = currentFlowOp->opOutFlow;
        doc["currentOperation"]["opTotalDeltaFlow"] = currentFlowOp->opTotalDeltaFlow;
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
    if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
      doc["success"] = false;
      doc["error"] = "Can't reset while an op is in progress";
    } else if (isnan(value) || value <= -1) {
      doc["success"] = false;
      doc["error"] = "invalid value";
    } else {
      if (strcmp(field, "litersIn")) {
        if (value == -1) {
          value = inFlowMeter.getLitersSinceReset();
        }
        inFlowMeter.resetFrom(value);
        doc["success"] = true;
      } else if (strcmp(field, "litersOut")) {
        if (value == -1) {
          value = inFlowMeter.getLitersSinceReset();
        }
        outFlowMeter.resetFrom(value);
        doc["success"] = true;
      } else {
        doc["success"] = false;
        doc["error"] = "unknown field";
      }
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
  inValve.setOn(false);
  outValve.setOn(false);
  growbotCommonSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD, []() {
            dbg.println("Update starting, turning off valves");
            inValve.setOn(false);
            outValve.setOn(false);
        });  
  dbg.println("Starting websocket...");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

}
unsigned long last = millis();
void loop() {
  growbotCommonLoop();
  webSocket.loop();  
  inValve.update();
  outValve.update();
  waterLevel.update();
  //peace of mind, always shut the input valve off if water level is 100 or more
  if (waterLevel.getWaterLevel() >= 100 && inValve.isOn()) {
    dbg.eprintln("Water level is at max but input valve is on!  Turning it off");
    inValve.setOn(false);
  }
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
      broadcastOpComplete();
    }
  }
  if (millis() - last > 5000) {
    dbg.printf("avg water level: %f\n", waterLevel.getWaterLevel());
    dbg.printf("In Flow: %f\n", inFlowMeter.getLitersSinceReset());
    dbg.printf("In ticks: %d\n", inFlowMeter.getPulseCount());
    dbg.printf("Out Flow: %f\n", outFlowMeter.getLitersSinceReset());
    dbg.printf("Out ticks: %d\n", outFlowMeter.getPulseCount());
    dbg.printf("wifi RSSI: %d\n", WiFi.RSSI());
    broadcastStatus();
    last = millis();
  }
  
}