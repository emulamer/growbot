#include <Arduino.h>
#include "FlowMsgs.h"
#include "SwitcherooMsgs.h"
#include "GbMsg.h"


GbMsg* parseGbMsg(char* payload, int length) {
  StaticJsonDocument<512> doc;
  auto err = deserializeJson(doc, payload, length);
  if (err) {
    dbg.printf("Error deserializing gb message: %s, data is %s\n", err.c_str(), (char*) payload);
    return NULL;
  }

  if (doc["msgType"].as<String>().equals(NAMEOF(SwitcherooStatusMsg))) {
    dbg.println("switcheroo status message");
    return new SwitcherooStatusMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(SwitcherooSetPortsMsg))) {
    dbg.println("switcheroo set port message");
    return new SwitcherooSetPortsMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(FlowStatusGbMsg))) {
    dbg.println("Flow Status message");
    return new FlowStatusGbMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(FlowResetCounterMsg))) {
    dbg.println("Flow reset counter message");
    return new FlowResetCounterMsg(doc);
  }  else if (doc["msgType"].as<String>().equals(NAMEOF(FlowStartOpMsg))) {
    dbg.println("Flow start op message");
    return new FlowStartOpMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(FlowAbortOpMsg))) {
    dbg.println("Flow abort op message");
    return new FlowAbortOpMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(FlowOpStartedMsg))) {
    dbg.println("Flow op started message");
    return new FlowOpStartedMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(FlowOpEndedMsg))) {
    dbg.println("Flow op ended message");
    return new FlowOpEndedMsg(doc);
  } else if (doc["msgType"].as<String>().equals(NAMEOF(GbGetStatusMsg))) {
    dbg.println("get status message");
    return new GbGetStatusMsg(doc);
  } else {
    dbg.println("Unknown message");
      return NULL;
  }
}
