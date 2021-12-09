#define GB_NODE_TYPE "growbot-ducter"
#define MDNS_NAME "growbot-ducter"
#define GB_NODE_ID MDNS_NAME
#define LOG_UDP_PORT 44450
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <EzEsp.h>
//#include <WebSocketsServer.h>
#include <WSMessenger.h>
#include <TempSensorMsgs.h>
#include <Servo.h>
#include <EEPROM.h>
#include <DucterMsgs.h>
#define SWITCHEROO_LIGHTS_PORT 1 
#define INSIDE_DUCT_PIN 3
#define OUTSIDE_DUCT_PIN 1
#define ADJUST_FREQ_MS 50000
#define IN_TEMP_SENSOR "growbot-inside-intake-temp"
#define OUT_TEMP_SENSOR "growbot-outside-intake-temp"

#define MIX_STABILIZATION_TIME 60000
#define AMBIENT_STABILIZATION_TIME 120000

#define LIGHT_TEMP_MIX_DIFF 5.0

float inTemp = NAN;
float outTemp = NAN;

float mixAirOffset  

#define ADJUST_THRESHOLD 0.15

#define STABLIZE_THRESHOLD 0.1

#define AMBIENT_SENSOR_NAME "ambient-temp"
#define MIX_SENSOR_NAME "growbot-mix-intake-temp"
///String targetSensorName = "";
float insidePct = 100;
float outsidePct = 0;
float targetTempC = 20;
float ambientTemp = NAN;
float mixTemp = NAN;
int currentMode = DUCTER_MODE_MANUAL;
bool lightsOn = false;


float mixAmbientOffset = 0;

#define ADJUST_STAGE_NONE 0
#define ADJUST_STAGE_MIX 1
#define ADJUST_STAGE_AMBIENT 2

int adjustStage = 0;

float getTargetMixTemp() {
  if (isnan(ambientTemp) || isnan(mixTemp) || isnan(targetTempC)) {
    return NAN;
  }
  if (lightsOn) {
    //if the lights are on, mix temp should be below the target temp
  } else {
    //if the lights are off, mix temp should be targeted at target temp (future: smart determination)
    return targetTempC;
  }
}

void adjust() {
  if (adjustStage == ADJUST_STAGE_NONE) {
    if (!isnan(mixTemp) && !isnan(ambientTemp)) {
      mixAmbientOffset = mixTemp - ambientTemp;
    }
    if (abs(ambientTemp - targetTempC) > ADJUST_THRESHOLD) {
      //adjustment needed
    } else {
      //things is fine
    }
  } else if (adjustStage == ADJUST_STAGE_MIX) {

  } else if (adjustStage == ADJUST_STAGE_AMBIENT) {

  }
}


unsigned long lastTick = 0;

UdpMessengerServer server(45678);
Servo insideServo;
Servo outsideServo;
#define MIN_SERVO 15
#define MAX_SERVO 180

void broadcastStatus() {
  DucterStatusMsg msg(currentMode, insidePct, outsidePct, targetTempC);
  server.broadcast(msg);
}

void setServos() {
  int inVal = ((MAX_SERVO - MIN_SERVO) * (insidePct/100.0)) + MIN_SERVO;
  int outVal = ((MAX_SERVO - MIN_SERVO) * (outsidePct/100.0)) + MIN_SERVO;
  dbg.printf("setting in servo: %d, out: %d\n", inVal, outVal);
  if (outVal > inVal) {
    outsideServo.write(outVal);
    insideServo.write(inVal);
  } else {
    insideServo.write(inVal);
    outsideServo.write(outVal);
  }
}

bool update() {
  if (currentMode == DUCTER_MODE_MANUAL) {
    setServos();
    return true;
  } else if (currentMode == DUCTER_MODE_AUTO) {
    bool isOk = false;
    //figure out logic to balance stuff
    if (isnan(ambientTemp)) {
      insidePct = 100;
      outsidePct = 0;
      dbg.printf("current temp is null!\n");
    } else if (isnan(inTemp)) {
      insidePct = 100;
      outsidePct = 0;
      dbg.printf("in temp is null!\n");
    } else if (isnan(outTemp)) {
      insidePct = 100;
      outsidePct = 0;
      dbg.printf("out temp is null!\n");
    } else if (outTemp == inTemp) {
      dbg.printf("in and out temps equal!\n");
      isOk = true;
    } else {
      bool isOutLow = outTemp<=inTemp;
      float lowTemp = isOutLow?outTemp:inTemp;
      float highTemp = isOutLow?inTemp:outTemp;
      float* lowVent = isOutLow?&outsidePct:&insidePct;
      float* highVent = isOutLow?&insidePct:&outsidePct;
      // if (targetTempC > highTemp) {
      //   *highVent = 100;
      //   *lowVent = 0;
      // } else if (targetTempC < lowTemp) {
      //   *highVent = 0;
      //   *lowVent = 100;        
      // } else {
      float diff = targetTempC - ambientTemp;
      float absRange = (highTemp - lowTemp);

      float pct = diff/absRange;
      float absPct = abs(pct)*100.0;
      if (absPct < 1 ) {
        //do nothing, 1% is pretty good
        absPct = 0;
      } else if ( absPct > 20 ) {
        absPct = 10;
      }
      pct = copysign(absPct, pct);
      pct /=2 ;
      *lowVent -= pct;
      *highVent += pct;
      if (*lowVent < 0) {
        *lowVent = 0;
      } else if (*lowVent > 100) {
        *lowVent = 100;
      }
      if (*highVent < 0) {
        *highVent = 0;
      } else if (*highVent > 100) {
        *highVent = 100;
      }
      //}
      dbg.printf("setting out vent to %f, in vent to %f\n", outsidePct, insidePct);
      isOk = true;
    }
    setServos();
    return isOk;
  }
  return false;
}

void tempMsg(MessageWrapper& mw) {
  TempSensorStatusMsg* msg = (TempSensorStatusMsg*)mw.message;
  if (msg->nodeId().equals(AMBIENT_SENSOR_NAME)) {
    ambientTemp = msg->tempC();
  } else if (msg->nodeId().equals(MIX_SENSOR_NAME)) {
    mixTemp = msg->tempC();
  } else if (msg->nodeId().equals(IN_TEMP_SENSOR)) {
    inTemp = msg->tempC();
  } else if (msg->nodeId().equals(OUT_TEMP_SENSOR)) {
    outTemp = msg->tempC();
  }
}
void setMsg(MessageWrapper& mw) {

  DucterSetDuctsMsg* msg = (DucterSetDuctsMsg*)mw.message;
  GbResultMsg repl;
  int mode = msg->mode();
  float inp = msg->insideOpenPercent();
  float outp = msg->outsideOpenPercent();
  float targetTemp = msg->targetTempC();
  // String sensorName = msg->targetSensorName();
  bool isOk = true;
  if (mode == DUCTER_MODE_MANUAL) {
    if (isnan(inp)) {
      repl.setUnsuccess("insideOpenPercent required for manual mode");
      isOk = false;
    } else if (isnan(outp)) {
      repl.setUnsuccess("outsideOpenPercent required for manual mode");
      isOk = false;
    }
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
  } else if (mode == DUCTER_MODE_AUTO) {
    if (isnan(targetTemp)) {
      repl.setUnsuccess("targetTempC required for auto mode");
      isOk = false;
    }
  } else {
    repl.setUnsuccess("invalid mode");
    isOk = false;
  }

  if (isOk) {
    currentMode = mode;

    if (currentMode == DUCTER_MODE_MANUAL)
    {
      insidePct = inp;
      outsidePct = outp;
      EEPROM.put<float>(0, insidePct);
      EEPROM.put<float>(4, outsidePct);  
    }
    if (!isnan(targetTemp)) {
      targetTempC = targetTemp;
    }
    // if (!(sensorName.length() < 1 || sensorName.equals("null"))) {
    //   //sensor name change, reconfigure stuff
    //   targetSensorName = sensorName;
    //   //checkSocketClient();
    //   currentTemp = NAN;
    // }
    EEPROM.put<int>(12, currentMode);
    EEPROM.put<float>(16, targetTempC);

    // int len = targetSensorName.length();
    // EEPROM.put(32, len);
    // const char* str = targetSensorName.c_str();
    // for (int i = 0; i < len; i++) {
    //   EEPROM.write(36 + i, str[i]);
    // }
    EEPROM.commit();
    repl.setSuccess();
  }
  update();
  repl.setSuccess();
  mw.reply(repl);
}

void switcherooMsg(MessageWrapper& mw) {
  SwitcherooStatusMsg* msg = (SwitcherooStatusMsg*)mw.message;
  lightsOn = msg->ports().portStatus[SWITCHEROO_LIGHTS_PORT];
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
  EEPROM.get<int>(12, currentMode);
  EEPROM.get<float>(16, targetTempC);
  
  setServos();
  server.onMessage(NAMEOF(DucterSetDuctsMsg), setMsg);
  server.onMessage(NAMEOF(TempSensorStatusMsg), tempMsg);
  server.onMessage(NAMEOF(SwitcherooStatusMsg), switcherooMsg);
  server.init();
}



unsigned long lastAdjust = 0;
void loop() {
  ezEspLoop();
  server.handle();
  if (millis() - lastAdjust > ADJUST_FREQ_MS) {
    dbg.println("adjusting");
    if (update()) {
      lastAdjust = millis();
    } else {
      //update 4x quicker if it fails
      lastAdjust = millis() - (ADJUST_FREQ_MS * 0.75);
    }
  }
  if (millis() - lastTick > 5000) {
    dbg.println("broadcast");
    broadcastStatus();
    dbg.printf("mode: %d, intemp: %f, outtemp: %f, tartemp: %f, inpct:%f, outpct:%f, curtmp: %f, mixtemp: %f, lights: %d\n", currentMode, inTemp, outTemp, targetTempC, insidePct, outsidePct, ambientTemp, mixTemp, lightsOn);
    lastTick = millis();
  }


}