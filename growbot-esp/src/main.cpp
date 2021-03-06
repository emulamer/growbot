#define LOG_UDP_PORT 44444
#define GB_NODE_TYPE "growbot-esp"
#define GB_NODE_ID "growbot-esp"
#include <EzEsp.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include "GrowbotData.h"
#include "DataConnection.h"
#include "Config.h" //all the config stuff is in here
#include <DebugUtils.h>
#include <Wire.h>
#include <stdlib.h>
#include <stdio.h>
#include "Sensorama.h"
#include "ReadingNormalizer.h"
#include "perhip/WaterTempSensor.h"
#include "perhip/LuxSensor.h"
#include "perhip/WSWaterLevel.h"
#include "perhip/PhSensor.h"
#include "perhip/ConductivitySensor.h"
#include "perhip/TempHumiditySensor.h"
#include "perhip/PowerControl.h"
#include "Thermostat.h"
#include "perhip/SwitcherooWiFi.h"
#include "TimeKeeper.h" 
#include "soc/rtc_wdt.h"
#include <driver/periph_ctrl.h>
#include "MQTTDataConnection.h"
#include "EEPROMNVStore.h"
#include "PumpControl.h"
#include <esp_task_wdt.h>
#include <Ticker.h>
#include <StaticServer.h>
MQTTDataConnection dataConn(MQTT_HOST, MQTT_PORT, MQTT_TOPIC, MQTT_CONFIG_TOPIC);
EEPROMNVStore nvStore;

#define EC_CALIBRATION_LOW 12880
#define EC_CALIBRATION_HIGH 80000
#define PH_CALIBRATION_MID 7
#define PH_CALIBRATION_LOW 4
#define PH_CALIBRATION_HIGH 10

#define GROWBOT_MODE_NORMAL 0x0
#define GROWBOT_MODE_CALIBRATING_PH_SENSOR 0x20 
#define GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID 0x21
#define GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW 0x22
#define GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_HIGH 0x23
#define GROWBOT_MODE_CALIBRATING_TDS_SENSOR 0x30
#define GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY 0x31
#define GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW 0x32
#define GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_HIGH 0x33
#define GROWBOT_MODE_REBOOT 0x99



GrowbotConfig config;
GrowbotData data;
GrowbotState state;

TwoWire* i2cBus = &Wire;
TwoWire* i2cBus2 = &Wire1;
I2CMultiplexer i2cMultiplexer(i2cBus, i2cBus2, I2C_MULTIPLEXER2_ADDRESS);
PowerControl powerCtl(i2cBus, I2C_POWER_CTL_ADDR);
SwitcherooWiFi* switcheroo = new SwitcherooWiFi();
TimeKeeper* lightsTimekeeper = new TimeKeeper(switcheroo, SWITCHEROO_LIGHTS_PORT, &config.lightSchedule, &data.lightsOn);
TimeKeeper* roomFansTimekeeper = new TimeKeeper(switcheroo, SWITCHEROO_ROOM_FAN_PORT, &config.roomFanSchedule, &data.roomFanOn);
PumpControl* pumpControl = new PumpControl(switcheroo, SWITCHEROO_PUMP_PORT, &data, &config.pumpOn);


FixedThermostat chillerThermostat;
int operating_mode = GROWBOT_MODE_NORMAL;


void sendState() {
    state.config = config;
    state.data = data;
    state.current_mode = operating_mode;
    if (!dataConn.sendState(state)) {
      dbg.wprintf("State was not sent!\n");
    }
    dbg.dprintf("sending state, lights flag is: %d\n", state.data.lightsOn);
}
void doImportantTicks();
Sensorama sensorama = Sensorama(i2cBus, i2cBus2, &i2cMultiplexer, &data, &config, doImportantTicks);
void doImportantTicks() {
  bool doSendState = false;
  ezEspLoop();
  doSendState = doSendState || lightsTimekeeper->handle();
  doSendState = doSendState || roomFansTimekeeper->handle();
  powerCtl.handle();
  dataConn.handle();
  sensorama.handle();
  switcheroo->handle();
  server.handle();
  if (doSendState) {
    sendState();
  }
}

void debug_scan_i2c(TwoWire *wire, I2CMultiplexer* plex) {
  byte error, address; //variable for error and I2C address
  int nDevices;

  for (int i = 0; i < 16; i++) {
    int port = i;
        //  dbg.printf("port in %d\n", port);
    nDevices = 0;
    for (address = 1; address < 127; address++ )
    {

      plex->setBus(port);
      // The i2c_scanner uses the return value of
      // the Write.endTransmisstion to see if
      // a device did acknowledge to the address.
      wire->beginTransmission(address);
      error = wire->endTransmission();
      byte printaddr;
      if (address < 16) {
        printaddr =  0;
      } else {
        printaddr = address;
      }
      if (error == 0)
      {
        
        dbg.printf("I2C device found port %d at address %x\n", i, printaddr);
        // if (address < 16)
        //   dbg.print("0");
        // dbg.print(address, HEX);
        // dbg.println("  !");
        nDevices++;
      }
      else if (error == 4)
      {
        dbg.printf("Unknown error port %d at address %x\n", i, printaddr);
      }
    }
  }
  if (nDevices == 0)
    dbg.printf("No I2C devices found \n");
  else
    dbg.printf("done\n");
}

void debug_scan_i2c(TwoWire *wire) {
  byte error, address; //variable for error and I2C address
  int nDevices;

  for (int i = 0; i < 16; i++) {
    int port = i;
        //  dbg.printf("port in %d\n", port);
    nDevices = 0;
    for (address = 1; address < 127; address++ )
    {

      // The i2c_scanner uses the return value of
      // the Write.endTransmisstion to see if
      // a device did acknowledge to the address.
      wire->beginTransmission(address);
      error = wire->endTransmission();
      byte printaddr;
      if (address < 16) {
        printaddr =  0;
      } else {
        printaddr = address;
      }
      if (error == 0)
      {
        
        dbg.printf("I2C device found port %d at address %x\n", i, printaddr);
        // if (address < 16)
        //   dbg.print("0");
        // dbg.print(address, HEX);
        // dbg.println("  !");
        nDevices++;
      }
      else if (error == 4)
      {
        dbg.printf("Unknown error port %d at address %x\n", i, printaddr);
      }
    }
  }
  if (nDevices == 0)
    dbg.printf("No I2C devices found \n");
  else
    dbg.printf("done\n");
}

void sendSensorMessages() {
  TempSensorStatusMsg msg(data.ambientInternal1.temperatureC, data.ambientInternal1.humidity);
  msg.setNodeType("growbot-temp");
  msg.setNodeId("ambient-temp");
  server.broadcast(msg);
  LuxSensorStatusMsg msg2(data.luxAmbient1);
  msg2.setNodeType("growbot-lux");
  msg2.setNodeId("plant-top-lux");
  server.broadcast(msg2);
}
void updateFromConfig() {

  sensorama.configChanged();
  //set power calibration
  powerCtl.setPowerCalibration(POWER_EXHAUST_FAN_PORT, config.exhaustFanCalibration, false);
  delay(50);
  powerCtl.setPowerCalibration(POWER_INTAKE_FAN_PORT, config.intakeFanCalibration, false);
  delay(50);
  

  //set on/off toggles
  powerCtl.setPowerToggle(POWER_EXHAUST_FAN_PORT, config.exhaustFanOn);
  delay(50);
  powerCtl.setPowerToggle(POWER_INTAKE_FAN_PORT, config.intakeFanOn);
  delay(50);
  switcheroo->setPowerToggle(SWITCHEROO_BUBBLES_PORT, config.bubblesOn);
  data.bubblesOn = config.bubblesOn;

  //set power levels
  powerCtl.setChannelLevel(POWER_EXHAUST_FAN_PORT, config.exhaustFanPercent);
  delay(50);
  powerCtl.setChannelLevel(POWER_INTAKE_FAN_PORT, config.intakeFanPercent);
  delay(50);

  //set schedules
  lightsTimekeeper->handle(true);
  roomFansTimekeeper->handle(true);
 }

void reset_system()
{
    rtc_wdt_protect_off();      //Disable RTC WDT write protection
    //Set stage 0 to trigger a system reset after 1000ms
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 100);
    rtc_wdt_enable();           //Start the RTC WDT timer
    rtc_wdt_protect_on();       //Enable RTC WDT write protection
}


void onNewConfig(GrowbotConfig &newConfig) {
  dbg.printf("got new config update!\n");
  config = newConfig;
  nvStore.writeConfig(config);
  updateFromConfig();
  sendState();
}

void onModeChange(byte mode) {
  if (operating_mode == GROWBOT_MODE_NORMAL) {
    if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR) {
      dbg.dprintln("Entering pH calibration mode");
      operating_mode = GROWBOT_MODE_CALIBRATING_PH_SENSOR;
    } else if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
      dbg.dprintln("Entering EC calibration mode");
      operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR;
    } else if (mode == GROWBOT_MODE_REBOOT) {
      dbg.dprintln("Reset system mode received, resetting now");
        reset_system();
    } else {
      dbg.wprintf("Invalid mode transition from normal to mode %d\n", mode);
      return;
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID) {
        dbg.dprintln("in ph calibration mode, got message to set the mid point calibration");
        //todo: actual mid point calib
        if (!((PhSensor*)sensorama.getSensor("waterData.ph"))->calibrate(EZO_CALIBRATE_MID, PH_CALIBRATION_MID)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("pH calibration canceled");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from ph calibration start to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW) {
        dbg.println("in ph calibration mid set mode, got message to set the low point calibration");
        //todo: actual low point calib
        if (!((PhSensor*)sensorama.getSensor("waterData.ph"))->calibrate(EZO_CALIBRATE_LOW, PH_CALIBRATION_LOW)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.wprintln("pH calibration canceled.  calibration is busted now!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.wprintf("Invalid mode transition from ph calibration mid to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_HIGH) {
        dbg.dprintln("in ph calibration low set mode, got message to set the high point calibration");
        //todo: actual high point calibration here
        //switch back to normal mode from here
        if (!((PhSensor*)sensorama.getSensor("waterData.ph"))->calibrate(EZO_CALIBRATE_HIGH, PH_CALIBRATION_HIGH)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_NORMAL;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.wprintln("pH calibration canceled.  calibration is busted now!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.wprintf("Invalid mode transition from ph calibration low to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY) {
        dbg.dprintln("in ec calibration mode, got message to set the dry point calibration");
        //todo: actual ec dry calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_DRY, 0)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("EC calibration canceled");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.wprintf("Invalid mode transition from EC calibration start to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW) {
        dbg.dprintln("in ec calibration mode, got message to set the low point calibration");
        //todo: actual ec low calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_LOW, EC_CALIBRATION_LOW)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.wprintln("EC calibration canceled.. ec calibration is hosed!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.wprintf("Invalid mode transition from EC calibration dry to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_HIGH) {
        dbg.dprintln("in ec calibration mode, got message to set the high point calibration");
        //todo: actual ec high calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_HIGH, EC_CALIBRATION_HIGH)) {
          dbg.eprintln("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_NORMAL;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.wprintln("EC calibration canceled.. ec calibration is hosed!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.wprintf("Invalid mode transition from EC calibration low to %d\n", mode);
        return;
      }
  }
  sendState();
}

void updateThermostats() {
  chillerThermostat.handle();
}

void setupIO() {
  periph_module_disable(PERIPH_I2C0_MODULE);
  periph_module_disable(PERIPH_I2C1_MODULE);
  digitalWrite(I2C_SDA_PIN, 0);
  digitalWrite(I2C_SCL_PIN, 0);
  delay(100);
  digitalWrite(I2C_SDA_PIN, 1);
  digitalWrite(I2C_SCL_PIN, 1);
  delay(100);
  digitalWrite(I2C_2_SDA_PIN, 0);
  digitalWrite(I2C_2_SCL_PIN, 0);
  delay(100);
  digitalWrite(I2C_2_SDA_PIN, 1);
  digitalWrite(I2C_2_SCL_PIN, 1);
  delay(100);  
  periph_module_enable(PERIPH_I2C0_MODULE);
  periph_module_enable(PERIPH_I2C1_MODULE);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);  
  Wire1.begin(I2C_2_SDA_PIN, I2C_2_SCL_PIN, I2C_FREQ);
    chillerThermostat.init(&config.waterChillerThermostat, &data.controlBucket.temperatureC,
      [](bool isOn, float value) { 
        if (isOn) {
          dbg.dprintf("chiller thermostat is on \n");
          switcheroo->setPowerToggle(SWITCHEROO_CHILLER_PORT, true);
          data.waterChillerStatus = 100;
        } else {
          dbg.dprintf("chiller thermostat is off \n");
          switcheroo->setPowerToggle(SWITCHEROO_CHILLER_PORT, false);
          data.waterChillerStatus = 0;
        }
      }
    );
    switcheroo->init();
    ezEspSetup(MDNS_NAME, WIFI_SSID, WIFI_PASSWORD);
}

void setup() {  
  dbg.setLogLevel(LOG_LEVEL_DEBUG);
  Serial.begin(115200);
  nvStore.init();
  dbg.println("Growbot v.01 starting up...");
  setupIO();
  powerCtl.init();
  dbg.print("Loading config...");
  nvStore.readConfig(&config);
  updateFromConfig();
  dbg.println("loaded.");

  dbg.print("Sensors...");
  sensorama.init();
  dbg.println("initialized.");

  dbg.print("Starting data connection...");
  dataConn.onModeChange(onModeChange);
  dataConn.onNewConfig(onNewConfig);
  dataConn.init();
  dbg.println("started.");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  ezEspLoop();
  sensorama.update();
  delay(1000);
  server.init();
}
unsigned long nextTick = 0;
bool tickNow() {
  unsigned long now = millis();
  if (nextTick <= now) {
    nextTick = millis() + config.samplingIntervalMS;
    return true;
  }
    return false;
}



void loop() {
  doImportantTicks();
  if (operating_mode == GROWBOT_MODE_NORMAL) {
    if (tickNow()) {
      sensorama.update();
      updateThermostats();
      pumpControl->handle();
      sendState();
      sendSensorMessages();
      dbg.dprintf("Waiting to tick again for %d\n", config.samplingIntervalMS);
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_HIGH) {
    sensorama.updateOne("waterData.ph", false);
    //readWaterQualitySensors(true, false);
    sendState();
    delay(2000);
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_HIGH ) {
    sensorama.updateOne("waterData.tds", false);
    sendState();
    delay(2000);
  }
}