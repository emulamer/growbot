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
#include <Preferences.h>

#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <WSMessenger.h>
#include "MessageParser.h"
#include "DoserPump.h"
#include "DoserMsgs.h"

#define STEPS_PER_ML 4870  //4970 //4675
#define RETRACTION_ML 2.0
#define MAX_RPM 120
#define NUM_DOSERS 6
#define MAX_CONCURRENT_ACTIVE 1
#define MAX_ML_DOSE 1000

// #define TUBE_SENSOR_1_PIN 36 //(SP)
// #define TUBE_SENSOR_2_PIN 39 //(SW)
// #define TUBE_SENSOR_3_PIN 
// #define TUBE_SENSOR_4_PIN 35
// #define TUBE_SENSOR_5_PIN 34
// #define TUBE_SENSOR_6_PIN 15


#define TUBE_SENSOR_1_PIN 35 
#define TUBE_SENSOR_2_PIN 36 //(SP)
#define TUBE_SENSOR_3_PIN 23
#define TUBE_SENSOR_4_PIN 39 //(SW)
#define TUBE_SENSOR_5_PIN 22
#define TUBE_SENSOR_6_PIN 34


#define PUMP_1_DIR_PIN 0 //23
#define PUMP_1_STEP_PIN 2 //22
#define PUMP_1_SLEEP_PIN 21

#define PUMP_2_DIR_PIN 13
#define PUMP_2_STEP_PIN 15
#define PUMP_2_SLEEP_PIN 32

#define PUMP_3_DIR_PIN 19
#define PUMP_3_STEP_PIN 18
#define PUMP_3_SLEEP_PIN 5

#define PUMP_4_DIR_PIN 33
#define PUMP_4_STEP_PIN 25
#define PUMP_4_SLEEP_PIN 26

#define PUMP_5_DIR_PIN 17
#define PUMP_5_STEP_PIN 16
#define PUMP_5_SLEEP_PIN 4

#define PUMP_6_DIR_PIN 27
#define PUMP_6_STEP_PIN 14
#define PUMP_6_SLEEP_PIN 12


DoserPump* dosers[NUM_DOSERS];

Preferences prefs;

UdpMessengerServer server(45678);

void broadcastDoseStart(int port, float targetMl) {
  DoserStartedMsg ds(port, targetMl);
  server.broadcast(ds);
}

void broadcastDoseEnd(int port, float actualMl, bool success) {
  //todo: something with errMsg
  DoserEndedMsg de(port, actualMl, success);
  server.broadcast(de);
}

void broadcastContinuousStart(int port) {
  DoserContinuousStartedMsg ds(port);
  server.broadcast(ds);
}

void broadcastContinuousEnd(int port, float actualMl) {
  DoserContinuousEndedMsg de(port, actualMl);
  server.broadcast(de);
}

void doseEnd(int doserId, int doseType, bool aborted, long stepsDosed, float mlDosed) {
  dbg.printf("Doser %d end, doseType: %d, aborted: %d, stepsDosed: %d, mlDosed: %f\n", doserId, doseType, aborted, stepsDosed, mlDosed);
  if (doseType == 1) {
    //dose end
    broadcastDoseEnd(doserId, mlDosed, !aborted);
  } else if (doseType == 2) {
    //continuous end
    broadcastContinuousEnd(doserId, mlDosed);
  } else if (doseType == 3) {
    //retraction end, probably not relevant for a broadcast?
  }
    
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





void initDosers() {
  
  dosers[0] = new DoserPumpA4988(1, PUMP_1_DIR_PIN, PUMP_1_STEP_PIN, PUMP_1_SLEEP_PIN, TUBE_SENSOR_1_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);
  dosers[1] = new DoserPumpA4988(2, PUMP_2_DIR_PIN, PUMP_2_STEP_PIN, PUMP_2_SLEEP_PIN, TUBE_SENSOR_2_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);
  dosers[2] = new DoserPumpA4988(3, PUMP_3_DIR_PIN, PUMP_3_STEP_PIN, PUMP_3_SLEEP_PIN, TUBE_SENSOR_3_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);
  dosers[3] = new DoserPumpA4988(4, PUMP_4_DIR_PIN, PUMP_4_STEP_PIN, PUMP_4_SLEEP_PIN, TUBE_SENSOR_4_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);
  dosers[4] = new DoserPumpA4988(5, PUMP_5_DIR_PIN, PUMP_5_STEP_PIN, PUMP_5_SLEEP_PIN, TUBE_SENSOR_5_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);
  dosers[5] = new DoserPumpA4988(6, PUMP_6_DIR_PIN, PUMP_6_STEP_PIN, PUMP_6_SLEEP_PIN, TUBE_SENSOR_6_PIN, STEPS_PER_ML, MAX_RPM, doseEnd);

  
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

bool stopAll() {
  bool any = false;
  for (int i = 0; i < NUM_DOSERS; i++) {
    if (dosers[i]->stop()) {
      any = true;
    }
  }
  return any;
}
void readCalibrationPrefs() {
  dbg.println("Reading calibration prefs");
  prefs.begin("calib", true);
  for (int i = 0; i < NUM_DOSERS; i++) {
    String key1 = String(i+1) + "-b2s";
    String key2 = String(i+1) + "-s2d";
    long b2s = prefs.getLong(key1.c_str(), -1);
    long s2d = prefs.getLong(key2.c_str(), -1);
    if (b2s != -1 && s2d != -1) {
      dosers[i]->setCalibration(b2s, s2d);
      dbg.printf("Read calibration for doser %d, b2s: %ld, s2d: %ld\n", i+1, b2s, s2d);
    }
    else {
      dbg.printf("No calibration for doser %d!\n", i+1);
    }
  }
  prefs.end();
}
void calibrationEnded(int doserId, bool success, long bottle2SensorSteps, long sensor2DoseSteps, int errorCode, String errorMessage) {
  if (success) {
    prefs.begin("calib", false);
    String key1 = String(doserId) + "-b2s";
    String key2 = String(doserId) + "-s2d";
    prefs.putLong(key1.c_str(), bottle2SensorSteps);
    prefs.putLong(key2.c_str(), sensor2DoseSteps);

    prefs.end();
    dbg.printf("Calibration for doser %d saved, b2s: %ld, s2d: %ld\n", doserId, bottle2SensorSteps, sensor2DoseSteps);
  }
  DoserCalibrateEndedMsg msg(doserId, success, bottle2SensorSteps, sensor2DoseSteps, errorCode, errorMessage);
  server.broadcast(msg);
}


void onCalibrateMsg(MessageWrapper& mw) {
  GbResultMsg res;
  DoserCalibratePortMsg* smsg = (DoserCalibratePortMsg*)mw.message;
  dbg.printf("start calibrate port %d\n", smsg->port());
  int port = smsg->port();
  if (port < 1 || port > NUM_DOSERS) {
    res.setUnsuccess("Invalid port");
    mw.reply(res);
    return;
  }
  if (dosers[port-1]->startCalibrate(calibrationEnded)) {
    res.setSuccess();
  } else {
    dbg.println("Calibration failed to start");
    res.setUnsuccess("Failed to start calibration");
  }
  
  mw.reply(res);

}

void onEStopMsg(MessageWrapper& mw) {
  GbResultMsg res;
  DoserEStopMsg* smsg = (DoserEStopMsg*)mw.message;
  DoserEStopMsg* msg = (DoserEStopMsg*)mw.message;
  dbg.println("Got ESTOP message, stoping all dosing");
  stopAll();
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
  dosers[realPort]->startContinuousPump();
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
  server.onMessage(NAMEOF(DoserCalibratePortMsg), onCalibrateMsg);
  server.init();
  dosers[0]->init();
  dosers[1]->init();
  dosers[2]->init();
  dosers[3]->init();
  dosers[4]->init();
  dosers[5]->init();
  readCalibrationPrefs();
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
    dbg.printf(" sensor pin1 %d 2 is %d 3 is %d 4 is %d 5 is %d 6 is %d\n", digitalRead(TUBE_SENSOR_1_PIN), digitalRead(TUBE_SENSOR_2_PIN), digitalRead(TUBE_SENSOR_3_PIN), digitalRead(TUBE_SENSOR_4_PIN), digitalRead(TUBE_SENSOR_5_PIN), digitalRead(TUBE_SENSOR_6_PIN));
    // dbg.printf("udp port is %d\n", server.getPort());
    // dbg.printf("started is %d\n", server.isStarted());
    // dbg.printf("msgcount is %d\n", server.messageCount());
    // PRINT("Active dosing count: ");
    // PRINTLN(getActiveDosingCount());
  }
}