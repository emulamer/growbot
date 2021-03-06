#define MDNS_NAME "growbot-lightify"
#define GB_NODE_TYPE "growbot-lightify"
#define NO_SERIAL
#define GB_NODE_ID MDNS_NAME
#define LOG_UDP_PORT 44454
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <EzEsp.h>
#include <WSMessenger.h>
#include <LightifyMsgs.h>
#include <SwitcherooMsgs.h>
#include <EEPROM.h>
#include <LuxSensorMsgs.h>
#include <PID_v1.h>

#define LEDPWM_PIN 14
#define SWITCHEROO_LIGHTS_PORT 1 
#define LUX_MIN_RESOLUTION 1000
#define MAX_LUX 50000

#define ADJ_FREQ_MS 15000

#define MIN_PWM 0
#define MAX_PWM 880
float Kp=.003, Ki=.1, Kd=.5;

double Input, Output, Setpoint;
//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

unsigned long lastAdjust = 0;
unsigned long lastTick = 0;
UdpMessengerServer server(45678);
int currentMode = 0;
float targetLux = NAN;
float currentLightPercent = NAN;
float currentSensorLux = NAN;
bool isOn = false;
String targetSensorName = "";
int lastPwm = 0;
unsigned long lastSensorUpdateCalc;
unsigned long lastSensorUpdate;
void setLight() {

  if (currentMode == LIGHTIFY_MODE_AUTO && isOn ) {
    if (isnan(currentSensorLux) || isnan(targetLux)) {
        //something is missing, make no change
    } else {
      if (lastSensorUpdate != lastSensorUpdateCalc) {
        lastSensorUpdateCalc = lastSensorUpdate;
        Input = currentSensorLux/1000.0;
        Setpoint = targetLux/1000.0;
        if (myPID.Compute()) {
          dbg.printf("calced\n");
        } else {
          dbg.printf("no calced\n");
        }
        if (Output < 1) {
          currentLightPercent = 1;
        } else {
          currentLightPercent = Output;
        }
        lastAdjust = millis();
        
      }
      // float diff = targetLux - currentSensorLux;
      // //assume we're not getting better than +/- 1000 lux probably/
      // if (abs(diff) > LUX_MIN_RESOLUTION) {
      //   float diffpct = (diff/MAX_LUX)*0.8;
      //   if (abs(diffpct) > 0.001) {
      //     if (isnan(currentLightPercent)) {
      //       currentLightPercent = 0;
      //     }
      //     currentLightPercent += diffpct*100;
          
      //   }
      // }
    }
  }

  if (!isnan(currentLightPercent)) {
    if (currentLightPercent < 0) {
      currentLightPercent = 0;
    } else if (currentLightPercent > 100) {
      currentLightPercent = 100;
    }
    //inverted, more is less!
    int pwmVal = 0;
    if (currentLightPercent == 0) {
      //make sure off is as far off as it goes
      pwmVal = 1023;
    } else {
      pwmVal = MAX_PWM -  (MIN_PWM  + ((MAX_PWM - MIN_PWM) * (currentLightPercent/100.0)));
    }
  lastPwm = pwmVal;
    ledcWrite(0, (int)(pwmVal));
  }
}

void broadcastStatus() {
  LightifyStatusMsg msg(currentMode, currentLightPercent, targetLux, targetSensorName);
  server.broadcast(msg);
}

void sensorMsg(MessageWrapper& mw) {
  LuxSensorStatusMsg* msg = (LuxSensorStatusMsg*)mw.message;
  String nodeId = msg->nodeId();
  if (!(nodeId != NULL && nodeId.equals(targetSensorName))) {
    return;
  }
  currentSensorLux = msg->lux();
  lastSensorUpdate = millis();
}

void switcherooMsg(MessageWrapper& mw) {
  SwitcherooStatusMsg* msg = (SwitcherooStatusMsg*)mw.message;
  isOn = msg->ports().portStatus[SWITCHEROO_LIGHTS_PORT];
  dbg.printf("got switcheroo msg, port is %d\n", isOn);
}

void setMsg(MessageWrapper& mw) {
  LightifySetMsg* msg = (LightifySetMsg*)mw.message;
  GbResultMsg repl;
  int mode = msg->mode();
  String sensorName = msg->sensor();
  float tlux = msg->targetLux();
  float lpct = msg->lightPercent();
  
  bool isOk = true;
  if (mode == LIGHTIFY_MODE_AUTO) {
    if (isnan(tlux)) {
      repl.setUnsuccess("targetLux is required for auto mode");
      isOk = false;
    } else if ((sensorName == NULL || sensorName.length() < 1 || sensorName.equals("null")) && (targetSensorName == NULL || targetSensorName.length() < 1 || targetSensorName.equals("null"))) {
      repl.setUnsuccess("targetSensorName is not set and must be provided for auto mode");
      isOk = false;
    }
  } else {
    if (isnan(lpct)) {
      if (currentLightPercent == NAN && mode == LIGHTIFY_MODE_MANUAL) {
        repl.setUnsuccess("lightPercent is required and unset for manual mode.");
        isOk = false;
      }
    } else {
      if (currentLightPercent < 0 || currentLightPercent > 100) {
        repl.setUnsuccess("lightPercent must be >= 0 and <= 100");
        isOk = false;
      } else {
        currentLightPercent = lpct;
      }      
    }
  }
  
  
  if (isOk) {
    currentMode = mode;
    if (!isnan(tlux)) {
      targetLux = tlux;
    } 
    if (!(sensorName.length() < 1 || sensorName.equals("null"))) {
      targetSensorName = sensorName;
      currentSensorLux = NAN;
    }
    EEPROM.put(0, currentMode);
    if (!isnan(targetLux)) {
      EEPROM.put(4, targetLux);
    }
    if (currentMode != LIGHTIFY_MODE_AUTO && !isnan(currentLightPercent)) {
      EEPROM.put(8, currentLightPercent);
    }
    
    int len = targetSensorName.length();
    EEPROM.put(32, len);
    const char* str = targetSensorName.c_str();
    for (int i = 0; i < len; i++) {
      EEPROM.write(36 + i, str[i]);
    }
    EEPROM.commit();
    repl.setSuccess();
  }  
  //dbg.printf("gonna do reply\n");
  mw.reply(repl);
}

void setup() {
  EEPROM.begin(512);
  myPID.SetOutputLimits(1.0, 100.0);
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(10000);
  pinMode(LEDPWM_PIN, OUTPUT);  
  digitalWrite(LEDPWM_PIN, LOW);
  ledcSetup(0, 1500, 10);
  ledcAttachPin(LEDPWM_PIN, 0);
  ezEspSetup(MDNS_NAME, "MaxNet", "88888888");
  EEPROM.get(0, currentMode);
  EEPROM.get(4, targetLux);  
  EEPROM.get(8, currentLightPercent);
  int len = 0;
  EEPROM.get(32, len);
  if (len >= 0 && len < 200) {
    char* str = (char*)malloc(len+1);
    str[len] = '\0';
    for (int i = 0; i < len; i++) {
      str[i] = EEPROM.read(36 + i);
    }
    dbg.printf("read string len %d: %s\n", len, str);
    targetSensorName = String(str);
    free(str);
  } else {
    dbg.printf("target sensor name didn't read good!\n");
    len = 0;
    EEPROM.put(32, len);
    targetSensorName = "";
  }
  dbg.printf("target sensor name: %s\n", targetSensorName.c_str());
  
  server.onMessage(NAMEOF(LightifySetMsg), setMsg);
  server.onMessage(NAMEOF(LuxSensorStatusMsg), sensorMsg);
  server.onMessage(NAMEOF(SwitcherooStatusMsg), switcherooMsg);
  server.init();
}



void loop() {
  ezEspLoop();
  server.handle();
  setLight();
  if (millis() - lastTick > 5000) {  
    dbg.printf("device %s, free heap: %d\n", MDNS_NAME, ESP.getFreeHeap());
    dbg.printf("pwm: %d, lightsOn: %d, target sensor: %s, sensorval: %f, currentLightPercent: %f, mode: %d, target: %f\n", lastPwm, isOn, targetSensorName.c_str(), currentSensorLux, currentLightPercent, currentMode, targetLux);
    
    broadcastStatus();
    lastTick = millis();
  }
}