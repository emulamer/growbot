#include <Arduino.h>
#include "FlowMsgs.h"
#include "SwitcherooMsgs.h"
#include "TempSensorMsgs.h"
#include "DucterMsgs.h"
#include "HumidifierMsgs.h"
#include "GbMsg.h"
#include "LuxSensorMsgs.h"
#include "LightifyMsgs.h"

#pragma once
GbMsg* parseGbMsg(char* payload, int length) {
  StaticJsonDocument<MSG_JSON_SIZE> doc;
  auto err = deserializeJson(doc, payload, length);
  if (err) {
    dbg.printf("Error deserializing gb message: %s, data is %s\n", err.c_str(), (char*) payload);
    return NULL;
  }
  String msgType = doc["msgType"].as<String>();
  if (msgType != NULL) {
    if (msgType.equals(NAMEOF(SwitcherooStatusMsg))) {
      return new SwitcherooStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(SwitcherooSetPortsMsg))) {
      return new SwitcherooSetPortsMsg(doc);
    } else if (msgType.equals(NAMEOF(FlowStatusMsg))) {
      return new FlowStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(FlowResetCounterMsg))) {
      return new FlowResetCounterMsg(doc);
    }  else if (msgType.equals(NAMEOF(FlowStartOpMsg))) {
      return new FlowStartOpMsg(doc);
    } else if (msgType.equals(NAMEOF(FlowAbortOpMsg))) {
      return new FlowAbortOpMsg(doc);
    } else if (msgType.equals(NAMEOF(FlowOpStartedMsg))) {
      return new FlowOpStartedMsg(doc);
    } else if (msgType.equals(NAMEOF(FlowOpEndedMsg))) {
      return new FlowOpEndedMsg(doc);
    } else if (msgType.equals(NAMEOF(GbGetStatusMsg))) {
      return new GbGetStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(GbResultMsg))) { 
      return new GbResultMsg(doc);
    } else if (msgType.equals(NAMEOF(DucterSetDuctsMsg))) { 
      return new DucterSetDuctsMsg(doc);
    } else if (msgType.equals(NAMEOF(DucterStatusMsg))) { 
      return new DucterStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(TempSensorStatusMsg))) { 
      return new TempSensorStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(HumidifierSetMsg))) { 
      return new HumidifierSetMsg(doc);
    } else if (msgType.equals(NAMEOF(HumidifierStatusMsg))) { 
      return new HumidifierStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(LuxSensorStatusMsg))) { 
      return new LuxSensorStatusMsg(doc);
    } else if (msgType.equals(NAMEOF(LightifySetMsg))) { 
      return new LightifySetMsg(doc);
    } else if (msgType.equals(NAMEOF(LightifyStatusMsg))) { 
      return new LightifyStatusMsg(doc);
    } else {
        dbg.printf("Unknown message: %s\n", msgType.c_str());
        return NULL;
    }
  } else {
      dbg.println("Message missing msgType");
      return NULL;
  }
}
