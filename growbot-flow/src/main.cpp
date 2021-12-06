#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"
#define MDNS_NAME "growbot-flow"
#define LOG_UDP_PORT 44446
#define GB_NODE_TYPE "growbot-flow"
#define GB_NODE_ID MDNS_NAME
#include <EzEsp.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
//#include <WebSocketsServer.h>
#include "FlowMeter.h"
#include <ESP32AnalogRead.h>
#include "WaterLevel.h"
#include "FlowValve.h"
#include "SolenoidValve.h"
#include "ComplexValve.h"
#include "ToggleValve.h"
#include <ArduinoJson.h>
#include <ESPRandom.h>
#include "FlowOperation.h"
#include "FlowMsgs.h"
#include <WSMessenger.h>
#include "MessageParser.h"



//#define WEBSOCKET_PORT 8118

#define LOOP_ON_PIN 25
#define LOOP_OFF_PIN 26

WaterLevel waterLevel(35, 34, 500);
FlowMeter inFlowMeter(5, 1380);//1136);
FlowMeter outFlowMeter(21, 460); 

ToggleValve* loopValve = new ToggleValve(25, 26);
FlowValve* drainValve = new SolenoidValve(27, 300, 15);

FlowValve* inValve = new SolenoidValve(33, 300, 15);
FlowValve* outValve = new ComplexValve(drainValve, loopValve, true);
// FlowValve valve26(26, 5000, 255);
// FlowValve valve14(14, 5000, 255);
// FlowValve valve32(32, 5000, 255);
// FlowValve valve25(25, 5000, 255);
// FlowValve valve5(32, 500, 20);
// FlowValve valve6(25, 500, 20);


//connector side:  33 32  14??? something wrong, 32 always triggers
// other side: 33, 32, 25

FlowOp* currentFlowOp = NULL;

struct FlowParts {
  WaterLevel* waterLevel;
  FlowMeter* inFlowMeter;
  FlowMeter* outFlowMeter;
  FlowValve* inValve;
  FlowValve* outValve;
};

FlowParts flowParts;

//WebSocketsServer webSocket(WEBSOCKET_PORT);   
UdpMessengerServer server(45678);


void broadcastOpComplete() {
  if (currentFlowOp == NULL) {
    return;
  }
  FlowInfo fi;
  fi.currentOperation = currentFlowOp;
  fi.litersIn = inFlowMeter.getLitersSinceReset();
  fi.litersOut = outFlowMeter.getLitersSinceReset();
  fi.waterLevel = waterLevel.getWaterLevel();
  FlowOpEndedMsg msg(fi);
  server.broadcast(msg);
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
  FlowOpStartedMsg msg(fi);
  server.broadcast(msg);
}
void broadcastStatus() {
  FlowInfo info;
  info.litersIn = inFlowMeter.getLitersSinceReset();
  info.litersOut = outFlowMeter.getLitersSinceReset();
  info.waterLevel = waterLevel.getWaterLevel();
  info.inValveOpen = inValve->isOpen();
  info.outValveOpen = outValve->isOpen();
  info.currentOperation = currentFlowOp;
  FlowStatusMsg status(info);
  server.broadcast(status);
  dbg.dprintln("broadcast status");
}

void startOp(FlowOp* newOp) {
  if (currentFlowOp != NULL) {
    delete currentFlowOp;
  }
  currentFlowOp = newOp;
  currentFlowOp->start();
  broadcastOpStart();
}
void onStartOpMsg(MessageWrapper& mw) {
  GbResultMsg res;
  dbg.dprintln("got a start op message");
  if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
    dbg.println("tried to start another op with one in progress");
    res.setUnsuccess("Another operation is in progress");
  } else {
    FlowStartOpMsg* smsg = (FlowStartOpMsg*)mw.message;
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
          startOp(new DrainToPercentEnsured(&waterLevel, outValve, &inFlowMeter, &outFlowMeter, smsg->param(0)));
          res.setSuccess();
        }
      } else if (op.equals(FlowOpType::Empty)) {
        startOp(new EmptyOp(&waterLevel, outValve, &inFlowMeter, &outFlowMeter));
      } else if (op.equals(FlowOpType::FillToPercent)) {
        if (paramCtr < 1) {
          dbg.printf("didn't get enough parameters for FillToPercent op\n");
          res.setUnsuccess("FillToPercent requires the percent to drain to");
        } else {
          startOp(new FillToPercentEnsured(&waterLevel, inValve, &inFlowMeter, &outFlowMeter, smsg->param(0)));
          res.setSuccess();
        }
      } else if (op.equals(FlowOpType::Flush)) {
          startOp(new FlushOp(&waterLevel, inValve, outValve, &inFlowMeter, &outFlowMeter, paramCtr>0?smsg->param(0):1,paramCtr>1?smsg->param(1):15, paramCtr>2?smsg->param(2):300));
          res.setSuccess();
      } else if (op.equals(FlowOpType::FlushAndFillToPercent)) {
        if (paramCtr < 1) {
          dbg.printf("didn't get enough parameters for FlushAndFillToPercent op\n");
          res.setUnsuccess("FlushAndFillToPercent requires at least the final fill percent, and optionally the number of rinses, the rinse fill percent, and the amount of seconds to rinse");
        } else {
          startOp(new FlushAndFillToPercentOp(&waterLevel, inValve, outValve, &inFlowMeter, &outFlowMeter, smsg->param(0),paramCtr>1?smsg->param(1):1, paramCtr>2?smsg->param(2):15, paramCtr>3?(int)smsg->param(3):300));
          res.setSuccess();
        }
      } else {
        dbg.printf("Got unknown op to start: %s\n", smsg->op().c_str());
        res.setUnsuccess("Unknown op");
      }  
    }
  }
  mw.reply(res);
}
void onAbortOpMsg(MessageWrapper& mw) {
  GbResultMsg res;
  if (currentFlowOp == NULL || currentFlowOp->isDone()) {
    res.setUnsuccess("There is no operation in progress");
  } else {
    currentFlowOp->abort();
    broadcastOpComplete();
    res.setSuccess();
  }
  mw.reply(res);
}
void onResetCounterMsg(MessageWrapper& mw) {
  GbResultMsg res;
  FlowResetCounterMsg* smsg = (FlowResetCounterMsg*)mw.message;
  String ctr = smsg->counterName();
  if (ctr == NULL) {
    dbg.printf("counterName must be provided\n");
  } else if (ctr.equals("litersIn")) {
    float val = smsg->fromAmount();
    inFlowMeter.resetFrom((val <=0 )?inFlowMeter.getLitersSinceReset():val);
    res.setSuccess();
    broadcastStatus();
  } else if (ctr.equals("litersOut")) {
    float val = smsg->fromAmount();
    outFlowMeter.resetFrom((val <=0 )?outFlowMeter.getLitersSinceReset():val);
    res.setSuccess();
    broadcastStatus();
  } else {
    dbg.printf("Got unknown reset counterName: %s\n", smsg->counterName().c_str());
    res.setUnsuccess("Unknown counterName");
  }
  mw.reply(res);
}

void setup() {
  pinMode(LOOP_ON_PIN, OUTPUT);
  pinMode(LOOP_OFF_PIN, OUTPUT);
  digitalWrite(LOOP_ON_PIN, HIGH);
  digitalWrite(LOOP_OFF_PIN, LOW);
  flowParts.inFlowMeter = &inFlowMeter;
  flowParts.outFlowMeter = &outFlowMeter;
  flowParts.inValve = inValve;
  flowParts.outValve = outValve;
  flowParts.waterLevel = &waterLevel;
  Serial.begin(115200);
  inValve->setOpen(false);
  outValve->setOpen(false);
  ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD, []() {
            dbg.println("Update starting, turning off valves");
            inValve->setOpen(false);
            outValve->setOpen(false);
            if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
              dbg.println("Update starting, aborting operation");
              currentFlowOp->abort();
              broadcastOpComplete();
            }            
        });  
  server.onMessage(NAMEOF(FlowStartOpMsg), onStartOpMsg);
  server.onMessage(NAMEOF(FlowResetCounterMsg), onResetCounterMsg);
  server.onMessage(NAMEOF(FlowAbortOpMsg), onAbortOpMsg);
  server.init();
}
unsigned long last = millis();
void loop() {
  ezEspLoop();
  server.handle();  
  inValve->update();
  outValve->update();
  waterLevel.update();
  //peace of mind, always shut the input valve off if water level is 100 or more
  if (waterLevel.getWaterLevel() >= 100 && inValve->isOpen()) {
    dbg.eprintln("Water level is at max but input valve is on!  Turning it off");
    inValve->setOpen(false);
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
    dbg.printf("In valve open: %d\n", inValve->isOpen());
    dbg.printf("Out valve open: %d\n", outValve->isOpen());
    
    dbg.printf("wifi RSSI: %d\n", WiFi.RSSI());
    broadcastStatus();
    last = millis();
  }
  
}

void FlowStatusMsg::setupMsg(FlowInfo& status) {
            (*this)["status"]["litersIn"] = status.litersIn;
            (*this)["status"]["litersOut"] = status.litersOut;
            (*this)["status"]["waterLevel"] = status.waterLevel;
            (*this)["status"]["inValveOpen"] = status.inValveOpen;
            (*this)["status"]["outValveOpen"] = status.outValveOpen;
            if (status.currentOperation != NULL) {
                (*this)["status"]["currentOp"]["type"] = ((FlowOp*)status.currentOperation)->getType();
                (*this)["status"]["currentOp"]["status"] = ((FlowOp*)status.currentOperation)->isDone()?(((FlowOp*)status.currentOperation)->isAborted()?"aborted":(((FlowOp*)status.currentOperation)->isFailed()?"failed":"done")):"running";
                (*this)["status"]["currentOp"]["litersIn"] = ((FlowOp*)status.currentOperation)->opInFlow;
                (*this)["status"]["currentOp"]["litersOut"] = ((FlowOp*)status.currentOperation)->opOutFlow;
                (*this)["status"]["currentOp"]["litersDelta"] = ((FlowOp*)status.currentOperation)->opTotalDeltaFlow;
                (*this)["status"]["currentOp"]["errorMessage"] = ((FlowOp*)status.currentOperation)->getError();
                (*this)["status"]["currentOp"]["id"] = ((FlowOp*)status.currentOperation)->getId();
                for (auto p: ((FlowOp*)status.currentOperation)->getParams()) {
                  (*this)["status"]["currentOp"]["params"].add(p);
                }
            }
        }