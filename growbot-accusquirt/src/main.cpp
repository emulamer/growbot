#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"
#define MDNS_NAME "growbot-accusquirt"
#define LOG_UDP_PORT 44445
#define GB_NODE_TYPE "growbot-accusquirt"
#define GB_NODE_ID MDNS_NAME
#include <vector>
#include <EzEsp.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <WSMessenger.h>
#include "MessageParser.h"
#include "DoserPump.h"
#include "DoserMsgs.h"

#define STEPS_PER_ML 4675
#define RETRACTION_ML 2.0
#define MAX_RPM 160
#define NUM_DOSERS 6
#define MAX_CONCURRENT_ACTIVE 1
#define MAX_ML_DOSE 1000

DoserPump* dosers[NUM_DOSERS];

UdpMessengerServer server(45678);

void broadcastDoseStart(int port, float targetMl) {
  DoserStartedMsg ds(port, targetMl);
  server.broadcast(ds);
}

void broadcastDoseEnd(int port, float targetMl, float actualMl, bool success, bool isRetracted) {
  DoserEndedMsg de(port, actualMl, success, isRetracted);
  server.broadcast(de);
}

void broadcastContinuousStart(int port) {
  DoserContinuousStartedMsg ds(port);
  server.broadcast(ds);
}

void broadcastContinuousEnd(int port, float actualMl, bool isRetracted) {
  DoserContinuousEndedMsg de(port, actualMl, isRetracted);
  server.broadcast(de);
}

void doseEnd(int doserId, bool aborted, long stepsDosed, float mlDosed, bool isRetracted, bool wasContinuousPump) {
  dbg.printf("Doser %d end, aborted: %d, stepsDosed: %d, mlDosed: %f, isRetracted: %d, wasContinuousPump: %d\n", doserId, aborted, stepsDosed, mlDosed, isRetracted, wasContinuousPump);
  if (wasContinuousPump) {
    broadcastContinuousEnd(doserId, mlDosed, isRetracted);
  } else {
    broadcastDoseEnd(doserId, mlDosed, mlDosed, !aborted, isRetracted);
  }
  
  // PRINT("Doser #");
  // PRINT(doserId);
  // PRINT(" end, aborted: ");
  // PRINT(aborted);
  // PRINT(", stepsDosed: ");
  // PRINT(stepsDosed);
  // PRINT(", mlDosed: ");
  // PRINT(mlDosed);
  // PRINT(", is retracted: ");
  // PRINT(isRetracted);
  // PRINT(", was continuous pump: ");
  // PRINTLN(wasContinuousPump);
}


void initDosers() {
  dosers[0] = new DoserPumpA4988(1, 23, 22, 21, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
  dosers[1] = new DoserPumpA4988(2, 13, 15, 32, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
  dosers[2] = new DoserPumpA4988(3, 19, 18, 5, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
  dosers[3] = new DoserPumpA4988(4, 33, 25, 26, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
  dosers[4] = new DoserPumpA4988(5, 17, 16, 4, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
  dosers[5] = new DoserPumpA4988(6, 27, 14, 12, STEPS_PER_ML, MAX_RPM, true, RETRACTION_ML, true, doseEnd);
}


bool anyDosing() {
  for (int i = 0; i < NUM_DOSERS; i++) {
    if (dosers[i]->isPumping()) {
      return true;
    }
  }
  return false;
}


void broadcastStatus() {
  DoserStatusMsg status(anyDosing());
  server.broadcast(status); 
}

int getActiveDosingCount() {
  int ct = 0;
  for (int i = 0; i < NUM_DOSERS; i++) {
    if (dosers[i]->isPumping()) {
      ct++;
    }
  }
  return ct;
}

bool stopAll(bool skipRetraction) {
  bool any = false;
  for (int i = 0; i < NUM_DOSERS; i++) {
    if (dosers[i]->stop(skipRetraction)) {
      any = true;
    }
  }
  return any;
}




void onEStopMsg(MessageWrapper& mw) {
  GbResultMsg res;
  DoserEStopMsg* smsg = (DoserEStopMsg*)mw.message;
  DoserEStopMsg* msg = (DoserEStopMsg*)mw.message;
  dbg.println("Got ESTOP message, stoping all dosing");
  stopAll(msg->skipRetract());
  res.setSuccess();
  mw.reply(res);
}
void onStartDoseMsg(MessageWrapper& mw) {
  DoserDosePortMsg* smsg = (DoserDosePortMsg*)mw.message;
  dbg.dprintf("Got start dose message for port %d for %f ml\n", smsg->port(), smsg->targetMl());
  GbResultMsg res;
  int port = smsg->port();
  float ml = smsg->targetMl();
  if (port < 1 && port > NUM_DOSERS) {
    dbg.dprintln("Rejecting start dose message, invalid port");
    res.setUnsuccess("Invalid port");
    mw.reply(res);
    return;
  }
  if (ml <= 0 || ml > MAX_ML_DOSE)  {
    dbg.dprintln("Rejecting start dose message, invalid ml");
    res.setUnsuccess("Invalid ml");
    mw.reply(res);
    return;
  }
  int active = getActiveDosingCount();
  if (active >= MAX_CONCURRENT_ACTIVE) {
    dbg.dprintln("Rejecting start dose message, too many dosing operations in progress");
    res.setUnsuccess("Too many dosing operations in progress");
    mw.reply(res);
    return;
  }
  int realPort = port - 1;
  if (dosers[realPort]->isPumping()) {
    dbg.dprintln("Rejecting start dose message, port is already dosing");
    res.setUnsuccess("Port is already dosing");
    mw.reply(res);
    return;
  }
  dosers[realPort]->startDosing(ml);
  broadcastDoseStart(port, ml);
  res.setSuccess();
  //res.setUnsuccess("err");

  mw.reply(res);
}

void onStartContinuousMsg(MessageWrapper& mw) {
  DoserStartContinuousPortMsg* smsg = (DoserStartContinuousPortMsg*)mw.message;
  dbg.dprintf("Got start continuous port message for port %d \n", smsg->port());
  GbResultMsg res;
  int port = smsg->port();
  if (port < 1 && port > NUM_DOSERS) {
    dbg.dprintln("Rejecting start continuous message, invalid port");
    res.setUnsuccess("Invalid port");
    mw.reply(res);
    return;
  }
  
  int active = getActiveDosingCount();
  if (active >= MAX_CONCURRENT_ACTIVE) {
    dbg.dprintln("Rejecting start continuous message, too many operations in progress");
    res.setUnsuccess("Too many operations in progress");
    mw.reply(res);
    return;
  }
  int realPort = port - 1;
  if (dosers[realPort]->isPumping()) {
    dbg.dprintln("Rejecting start continuous message, port is already active");
    res.setUnsuccess("Port is already active");
    mw.reply(res);
    return;
  }
  dosers[realPort]->startContinuousPump(smsg->skipRetract());
  broadcastContinuousStart(port);
  res.setSuccess();
  //res.setUnsuccess("err");

  mw.reply(res);
}
// void onStartDoseMillisMsg(MessageWrapper& mw) {
//   GbResultMsg res;
//   DoserDosePortForMillisecMsg* msg = (DoserDosePortForMillisecMsg*)mw.message;
  
//   //res.setSuccess();
//   //res.setUnsuccess("err");

//   mw.reply(res);
// }


void setup() {
  //Serial.begin(115200);
  dbg.println("Starting up");
  initDosers();
  dbg.println("calling ezEspSetup");
  ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD,[]() {
            dbg.println("Update starting, stoping all dosing");
            //todo: abort dosing, send abort end msgs
            // inValve->setOpen(false);
            // outValve->setOpen(false);
            // if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
            //   dbg.println("Update starting, aborting operation");
            //   currentFlowOp->abort();
            //   broadcastOpComplete();
            // }            
        });
  server.onMessage(NAMEOF(DoserDosePortMsg), onStartDoseMsg);
  server.onMessage(NAMEOF(DoserStartContinuousPortMsg), onStartContinuousMsg);
  server.onMessage(NAMEOF(DoserEStopMsg), onEStopMsg);
 // server.onMessage(NAMEOF(DoserDosePortForMillisecMsg), onStartDoseMillisMsg);
  server.init();
}

int interval = 5000;
unsigned long lastMillis = 0;

void loop() {
  ezEspLoop();
  server.handle();
  for (int i = 0; i < NUM_DOSERS; i++) {
    dosers[i]->update();
  }
  if (millis() - lastMillis > interval) {
    broadcastStatus();
    lastMillis = millis();
    // dbg.printf("udp port is %d\n", server.getPort());
    // dbg.printf("started is %d\n", server.isStarted());
    // dbg.printf("msgcount is %d\n", server.messageCount());
    // PRINT("Active dosing count: ");
    // PRINTLN(getActiveDosingCount());
  }
}