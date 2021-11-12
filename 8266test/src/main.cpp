#define LOG_UDP_PORT 44411
#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <GrowbotCommon.h>
#include <DebugUtils.h>
#include <WiFiUdp.h>

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  growbotCommonSetup("ESPTEST", "MaxNet", "88888888");
}
unsigned long lastTick = millis();
void loop() {
  growbotCommonLoop();
  if (millis() - lastTick > 5000) {

    dbg.println("doing tick!");
    IPAddress addr;
    if (!Resolver.resolve("growbot-flow", &addr)) {
      dbg.println("failed to resolve!");
    } else {
      dbg.printf("resolved to: %s", addr.toString().c_str());
    }
    lastTick = millis();
  }
}