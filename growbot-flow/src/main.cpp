#define LOG_UDP_PORT 44446
#define GB_NODE_TYPE "growbot-flow"
#include <EzEsp.h>
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
#include "FlowMsgs.h"
#include "MessageParser.h"

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

struct FlowParts {
  WaterLevel* waterLevel;
  FlowMeter* inFlowMeter;
  FlowMeter* outFlowMeter;
  SolenoidValve* inValve;
  SolenoidValve* outValve;
};

FlowParts flowParts;

WebSocketsServer webSocket(WEBSOCKET_PORT);   


void broadcastOpComplete() {
  if (currentFlowOp == NULL) {
    return;
  }
  FlowInfo fi;
  fi.currentOperation = currentFlowOp;
  fi.litersIn = inFlowMeter.getLitersSinceReset();
  fi.litersOut = outFlowMeter.getLitersSinceReset();
  fi.waterLevel = waterLevel.getWaterLevel();
  FlowOpEndedMsg msg(WiFi.macAddress(), fi);
  String jsonStr = msg.toJson();
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
  FlowInfo fi;
  fi.currentOperation = currentFlowOp;
  fi.litersIn = inFlowMeter.getLitersSinceReset();
  fi.litersOut = outFlowMeter.getLitersSinceReset();
  fi.waterLevel = waterLevel.getWaterLevel();
  FlowOpStartedMsg msg(WiFi.macAddress(), fi);
  String jsonStr = msg.toJson();
  webSocket.broadcastTXT(jsonStr);
}
void broadcastStatus() {
  FlowInfo info;
  info.litersIn = inFlowMeter.getLitersSinceReset();
  info.litersOut = outFlowMeter.getLitersSinceReset();
  info.waterLevel = waterLevel.getWaterLevel();
  info.currentOperation = currentFlowOp;
  FlowStatusGbMsg status(WiFi.macAddress(), info);
  String jsonStr = status.toJson();
  webSocket.broadcastTXT(jsonStr);
  dbg.dprintln("broadcast status");
}

void startOp(FlowOp* newOp) {
  if (currentFlowOp != NULL) {
    delete currentFlowOp;
  }
  currentFlowOp = newOp;
  currentFlowOp->start();
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
  dbg.println("about to try parsing");
  GbMsg* msg = parseGbMsg((char*)payload, length);
  if (msg == NULL) {
    dbg.println("parsing failed, returned null");
    return;
  } else {
    dbg.printf("parsing success, type is %s\n", msg->myType().c_str());
  }
  GbResultMsg res(WiFi.macAddress());
  if (msg->msgId() != NULL) {
    res.setReplyToMsgId(msg->msgId());
  }
  String msgType = msg->msgType();
  if (msgType == NULL) {
    dbg.println("message with no message type");
    res.setUnsuccess("msgType was missing");

  } else if (msgType.equals(NAMEOF(FlowStartOpMsg))) {
    dbg.dprintln("got a start op message");
    if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
      dbg.println("tried to start another op with one in progress");
      res.setUnsuccess("Another operation is in progress");
    } else {
      FlowStartOpMsg* smsg = (FlowStartOpMsg*)msg;
      String op = smsg->op();      
      if (op == NULL || op.equals("null")) {
        dbg.printf("op must be provided\n");
        res.setUnsuccess("op must be provided");
      } else  {
        int paramCtr = smsg->paramCount();
        if (op.equals(FlowOpType::DrainToPercent)) {
          if (paramCtr < 1) {
            dbg.printf("didn't get enough parameters for DrainToPercent op\n");
            res.setUnsuccess("DrainToPercent requires the percent to drain to");
          } else {
            startOp(new DrainToPercentOp(&waterLevel, &outValve, &inFlowMeter, &outFlowMeter, smsg->param(0)));
            res.setSuccess();
          }
        } else if (op.equals(FlowOpType::Empty)) {

        } else if (op.equals(FlowOpType::FillToPercent)) {
          if (paramCtr < 1) {
            dbg.printf("didn't get enough parameters for FillToPercent op\n");
            res.setUnsuccess("FillToPercent requires the percent to drain to");
          } else {
            startOp(new FillToPercentOp(&waterLevel, &inValve, &inFlowMeter, &outFlowMeter, smsg->param(0)));
            res.setSuccess();
          }
        } else if (op.equals(FlowOpType::Flush)) {
            startOp(new FlushOp(&waterLevel, &inValve, &outValve, &inFlowMeter, &outFlowMeter, paramCtr>0?smsg->param(0):1,paramCtr>1?smsg->param(1):15, paramCtr>2?smsg->param(2):300));
            res.setSuccess();
        } else if (op.equals(FlowOpType::FlushAndFillToPercent)) {
          if (paramCtr < 1) {
            dbg.printf("didn't get enough parameters for FlushAndFillToPercent op\n");
            res.setUnsuccess("FlushAndFillToPercent requires at least the final fill percent, and optionally the number of rinses, the rinse fill percent, and the amount of seconds to rinse");
          } else {
            startOp(new FlushAndFillToPercentOp(&waterLevel, &inValve, &outValve, &inFlowMeter, &outFlowMeter, smsg->param(0),paramCtr>1?smsg->param(1):1, paramCtr>2?smsg->param(2):15, paramCtr>3?(int)smsg->param(3):300));
            res.setSuccess();
          }
        } else {
          dbg.printf("Got unknown op to start: %s\n", smsg->op().c_str());
          res.setUnsuccess("Unknown op");
        }  
      }
    }
  } else if (msgType.equals(NAMEOF(FlowResetCounterMsg))) {
    dbg.dprintln("got restart counter message");
    FlowResetCounterMsg* smsg = (FlowResetCounterMsg*)msg;
    String ctr = smsg->counterName();
    if (ctr == NULL) {
      dbg.printf("counterName must be provided\n");
    } else if (ctr.equals("inLiters")) {
      float val = smsg->fromAmount();
      inFlowMeter.resetFrom((val <=0 )?inFlowMeter.getLitersSinceReset():val);
      res.setSuccess();
    } else if (ctr.equals("outLiters")) {
      float val = smsg->fromAmount();
      outFlowMeter.resetFrom((val <=0 )?outFlowMeter.getLitersSinceReset():val);
      res.setSuccess();
    } else {
      dbg.printf("Got unknown reset counterName: %s\n", smsg->counterName().c_str());
      res.setUnsuccess("Unknown counterName");
    }
  } else if (msgType.equals(NAMEOF(FlowAbortOpMsg))) {
    dbg.dprintln("got abort op message");
    if (currentFlowOp == NULL || currentFlowOp->isDone()) {
      res.setUnsuccess("There is no operation in progress");
    } else {
      currentFlowOp->abort();
      res.setSuccess();
    }
  } else if (msgType.equals(NAMEOF(GbGetStatusMsg))) {
    broadcastStatus();
    res.setSuccess();
  } else {
    dbg.dprintf("got unknown msg: %s\n", payload);
    res.setUnsuccess("Unknown message type");
  }
  String json = res.toJson();
  webSocket.sendTXT(num, json);
  delete msg;
}



void setup() {
  flowParts.inFlowMeter = &inFlowMeter;
  flowParts.outFlowMeter = &outFlowMeter;
  flowParts.inValve = &inValve;
  flowParts.outValve = &outValve;
  flowParts.waterLevel = &waterLevel;
  Serial.begin(115200);
  inValve.setOn(false);
  outValve.setOn(false);
  ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD, []() {
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
  ezEspLoop();
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
    dbg.printf("free heap: %d\n", ESP.getFreeHeap());
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

FlowStatusGbMsg::FlowStatusGbMsg(String nodeId, FlowInfo& status) : GbMsg(NAMEOF(FlowStatusGbMsg), nodeId) {
            (*this)["status"]["litersIn"] = status.litersIn;
            (*this)["status"]["litersOut"] = status.litersOut;
            (*this)["status"]["waterLevel"] = status.waterLevel;
            if (status.currentOperation != NULL) {
                (*this)["status"]["currentOp"]["type"] = ((FlowOp*)status.currentOperation)->getType();
                (*this)["status"]["currentOp"]["status"] = ((FlowOp*)status.currentOperation)->isDone()?(((FlowOp*)status.currentOperation)->isAborted()?"aborted":(((FlowOp*)status.currentOperation)->isFailed()?"failed":"done")):"running";
                (*this)["status"]["currentOp"]["litersIn"] = ((FlowOp*)status.currentOperation)->opInFlow;
                (*this)["status"]["currentOp"]["litersOut"] = ((FlowOp*)status.currentOperation)->opOutFlow;
                (*this)["status"]["currentOp"]["litersDelta"] = ((FlowOp*)status.currentOperation)->opTotalDeltaFlow;
                (*this)["status"]["currentOp"]["errorMessage"] = ((FlowOp*)status.currentOperation)->getError();
            }
        }