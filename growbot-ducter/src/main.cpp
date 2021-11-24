#define GB_NODE_TYPE "growbot-ducter"
#define MDNS_NAME "growbot-ducter"
#define LOG_UDP_PORT 44450
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <EzEsp.h>
#include <WebSocketsServer.h>
#include <WSMessenger.h>
#include <TempSensorMsgs.h>
#include <Servo.h>
#include <EEPROM.h>
#include <DucterMsgs.h>

#define INSIDE_DUCT_PIN 1
#define OUTSIDE_DUCT_PIN 3


float insidePct = 100;
float outsidePct = 0;

unsigned long lastTick = 0;

MessengerServer server(WiFi.macAddress(), 8118);
Servo insideServo;
Servo outsideServo;
#define MIN_SERVO 15
#define MAX_SERVO 180

void broadcastStatus() {
  DucterStatusMsg msg(WiFi.macAddress(), insidePct, outsidePct);
  server.broadcast(msg);
}

void setServos() {
  int inVal = ((MAX_SERVO - MIN_SERVO) * (insidePct/100.0)) + MIN_SERVO;
  int outVal = ((MAX_SERVO - MIN_SERVO) * (outsidePct/100.0)) + MIN_SERVO;
  dbg.printf("setting in servo: %d, out: %d\n", inVal, outVal);
 insideServo.write(inVal);
 outsideServo.write(outVal);
}

void setMsg(MessageWrapper& mw) {

  DucterSetDuctsMsg* msg = (DucterSetDuctsMsg*)mw.message;
  GbResultMsg repl(WiFi.macAddress());
  float inp = msg->insideOpenPercent();
  float outp = msg->outsideOpenPercent();
  dbg.printf("got set ducts msg, inside: %f, outside %f\n", inp, outp);
  if (isnan(inp)) {
    repl.setUnsuccess("insideOpenPercent is required");
  } else if (isnan(outp)) {
    repl.setUnsuccess("outsideOpenPercent is required");
  } else {
    if (inp < 0) {
      inp = 0;
    } else if (inp > 100) {
      inp = 100;
    }
    if (outp < 0) {
      outp = 0;
    } else if (outp > 100) {
      outp = 100;
    }
    insidePct = inp;
    outsidePct = outp;
    EEPROM.put(0, insidePct);
    EEPROM.put(4, outsidePct);
    EEPROM.commit();
    setServos();
    repl.setSuccess();
  }
  mw.reply(repl);
}

void setup() {
  EEPROM.begin(512);    
  ezEspSetup(MDNS_NAME, "MaxNet", "88888888");
  Serial.begin(115200);  
  //GPIO 1 (TX) swap the pin to a GPIO.
  pinMode(INSIDE_DUCT_PIN, FUNCTION_3); 
  //GPIO 3 (RX) swap the pin to a GPIO.
  pinMode(OUTSIDE_DUCT_PIN, FUNCTION_3); 
  insideServo.attach(INSIDE_DUCT_PIN);
  outsideServo.attach(OUTSIDE_DUCT_PIN);
  EEPROM.get<float>(0, insidePct);
  EEPROM.get<float>(4, outsidePct);
  setServos();
  server.onMessage(NAMEOF(DucterSetDuctsMsg), setMsg);
}

void loop() {
  ezEspLoop();
  server.handle();
  if (millis() - lastTick > 5000) {
    broadcastStatus();
    lastTick = millis();
  }
   


}