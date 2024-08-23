

#define WIFI_SSID "MaxNet"
#define WIFI_PASSWORD "88888888"

#define GB_NODE_TYPE "growbot-powermon"
#define GB_NODE_ID MDNS_NAME
//MDNS name and log udp port are set via build variables

#define BROADCAST_FREQ_MS 5000

#define SCL_PIN 23
#define SDA_PIN 22

#define SENSOR_AMPS_PER_VOLT 30.0

#include <vector>
#include <EzEsp.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <WSMessenger.h>
#include "MessageParser.h"
#include "PowerMonMsgs.h"
#include <Preferences.h>



#define AMP_READ_INTERVAL_MS 200
#define SAVE_DATA_INTERVAL_MS (60 * 60 * 1000) // 1 hour in milliseconds
#define BROADCAST_INTERVAL_MS 5000

double totalAmps = 0;  
double totalAmpHours = 0;
unsigned long totalAmpsResetUnixSeconds = 0;
unsigned long lastAmpsSavedStamp = 0;
unsigned long lastCheckedStamp = 0;
float lastReadAmps = 0;

#include "ADS1X15.h"

ADS1115* ADS;

Preferences prefs;

UdpMessengerServer server(45678);

bool adsAlive = false; 

bool adsReadOk = false;

bool initAds() {

  ADS->setGain(4);
  ADS->readADC(0);
  ADS->setMode(1);
  ADS->readADC(0);
  ADS->setDataRate(7);
  if (!ADS->begin()) {
    return false;
  }
  return ADS->isConnected();
}

bool readAdsPeaks(int samples, int16_t* low, int16_t* hi) {
  *low = 0;
  *hi = 0;
  int16_t last = 0;
  int16_t cur = 0;
  int16_t curpeak = 0;
  int16_t curlow = 0;
  int dirChanges = 0;
  bool isUpward = true;
  unsigned long ms = millis();
  int sampleCt = (samples *2);
  while (dirChanges < sampleCt && millis() - ms < 1000) {
    ADS->requestADC_Differential_0_1();
    int ctr = 0;
    while (!ADS->isReady() && ctr < 100) {
      delay(1);
      ctr++;
    }
    if (!ADS->isReady()) {
      dbg.println("ADS not ready after 100ms, setting it to not ready");
      adsAlive = false;
      return false;
    }
    last = cur;
    cur = ADS->getValue();
    if (last < cur != isUpward) {
      dirChanges++;
      isUpward = !isUpward;
    }
    if (cur < curlow) {
      curlow = cur;
    } else if (cur > curpeak) {
      curpeak = cur;
    }
  }
  unsigned long elapsed = millis() - ms;
  if (dirChanges < sampleCt) {
    dbg.printf("ADS read failed, not enough dir changes, only %d in %lu\n", dirChanges, elapsed);
    adsAlive = false;
    return false;
  }
  *low = curlow;
  *hi = curpeak;
  return true;
}

#define ADS_AVG_SAMPLES 3

 bool readAds(float* storeTo) {
  *storeTo = 0;
  int16_t curpeak = 0;
  int16_t curlow = 0;
  float ampSum = 0;
  for (int i = 0; i < ADS_AVG_SAMPLES; i++) {
   
    if (!readAdsPeaks(4, &curlow, &curpeak)) {
      return false;
    }
    float peak = ADS->toVoltage(curpeak);
    float low = ADS->toVoltage(curlow);
    float peaks = (peak - low) /2.0;
    float rms = peaks / sqrt(2.0);
    float amps = rms * SENSOR_AMPS_PER_VOLT;
    ampSum += amps;
  }
  float amps = ampSum / (float)ADS_AVG_SAMPLES;
  //dbg.printf("amps: %f\n", amps);

  *storeTo = amps;  
  return true;
}

void broadcastStatus() {
  PowerMonStatusMsg status(adsReadOk && adsAlive, lastReadAmps, totalAmpHours, totalAmpsResetUnixSeconds);
  server.broadcast(status); 
}


void readPrefs() {
  prefs.begin("amps", true);
  totalAmpsResetUnixSeconds = prefs.getULong("resetTime", 0);
  totalAmps = prefs.getDouble("totalAmps", 0);
  prefs.end();
}

void writePrefs() {
  prefs.begin("amps", false);
  prefs.putULong("resetTime", totalAmpsResetUnixSeconds);
  prefs.putDouble("totalAmps", totalAmps);
  prefs.end();
}

void onCommandMsg(MessageWrapper& mw) {
  GbResultMsg res;
  PowerMonCommandMsg* smsg = (PowerMonCommandMsg*)mw.message;
  if (smsg->resetAmpHours()) {
    totalAmps = 0;
    totalAmpsResetUnixSeconds = smsg->unixSecondsStamp();
    writePrefs();
    broadcastStatus();
    res.setSuccess();
    dbg.dprintf("Got cmd msg, reset amp hours to 0 and unix stamp to %lu\n", totalAmpsResetUnixSeconds);
    mw.reply(res);
  } else {
    res.setUnsuccess("Unknown command");  
  }
  mw.reply(res);
}


void setup() {
  Wire.setPins(SDA_PIN, SCL_PIN);
  Wire.begin();
  ADS = new ADS1115(0x48);
  //Serial.begin(115200);
  dbg.println("Starting up");
  readPrefs();
  dbg.println("calling ezEspSetup");
  ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD,[]() {
            dbg.println("Update starting, persisting amps");
            writePrefs();
            //todo: abort dosing, send abort end msgs
            // inValve->setOpen(false);
            // outValve->setOpen(false);
            // if (currentFlowOp != NULL && !currentFlowOp->isDone()) {
            //   dbg.println("Update starting, aborting operation");
            //   currentFlowOp->abort();
            //   broadcastOpComplete();
            // }            
        });
  server.onMessage(NAMEOF(PowerMonCommandMsg), onCommandMsg);
  server.init();
  adsAlive = initAds();
}


unsigned long lastMillis = 0;

void loop() {
  ezEspLoop();
  server.handle();
  if (millis() - lastCheckedStamp > AMP_READ_INTERVAL_MS) {
    unsigned long millidiff = millis() - lastCheckedStamp;
    lastCheckedStamp = millis();
    if (adsAlive) {
       float adcRead;
       adsReadOk = readAds(&adcRead);
       //dbg.printf("ADC read: %f\n", adcRead);
       lastReadAmps = adcRead;
       totalAmps += adcRead;
       totalAmpHours += adcRead * (millidiff / 1000.0) / 3600.0;      
      //todo read it from the thing
      //lastReadAmps = amps;
      //totalAmps += amps;
      //broadcastStatus();
    }
  }
  if (millis() - lastAmpsSavedStamp > SAVE_DATA_INTERVAL_MS) {
    lastAmpsSavedStamp = millis();
    writePrefs();
    dbg.println("Saved amp record");
  }


  if (millis() - lastMillis > BROADCAST_INTERVAL_MS) {
    broadcastStatus();
    lastMillis = millis();
    //send bcast
    if (!adsAlive) {
      dbg.println("ADS is not alive, trying to init it again");
      adsAlive = initAds();
      dbg.println(adsAlive?"ADS is alive after init":"ADS is not alive after init");
    }
    broadcastStatus();
    dbg.printf(" ADS alive: %d, ads last read ok: %d, Instantaneous amp draw: %f, Total amp hours: %f from timestamp %lu \n", adsAlive, adsReadOk, lastReadAmps, totalAmpHours, totalAmpsResetUnixSeconds);
  }
}