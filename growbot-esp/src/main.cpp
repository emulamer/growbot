#include <Arduino.h>
#include <Wire.h>
#include "GrowbotData.h"
#include "DataConnection.h"
#include "Config.h" //all the config stuff is in here
#include "DebugUtils.h"
#include <Wire.h>
#include <stdlib.h>
#include <stdio.h>
#include "Sensorama.h"
#include "ReadingNormalizer.h"
#include "perhip/WaterTempSensor.h"
#include "perhip/LuxSensor.h"
#include "perhip/WaterLevel.h"
#include "perhip/PhSensor.h"
#include "perhip/ConductivitySensor.h"
#include "perhip/TempHumiditySensor.h"
#include "perhip/PowerControl.h"
#include "Thermostat.h"
#include "perhip/SwitcherooWiFi.h"
#include "TimeKeeper.h" 
#ifdef ARDUINO_ARCH_ESP32
#include <ArduinoOTA.h>
#include "soc/rtc_wdt.h"
#include <driver/periph_ctrl.h>
#include "WifiManager.h"
#include "MQTTDataConnection.h"
#include "EEPROMNVStore.h"
#include "PumpControl.h"
WifiManager wifiMgr(WIFI_SSID, WIFI_PASSWORD);
MQTTDataConnection dataConn(MQTT_HOST, MQTT_PORT, MQTT_TOPIC, MQTT_CONFIG_TOPIC);
EEPROMNVStore nvStore;
#endif
#ifdef ARDUINO_ARCH_RP2040
#include "SerialDataConnection.h"
#include "PINVStore.h"
SerialDataConnection dataConn = SerialDataConnection();
PINVStore nvStore;
#endif

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
//i2c stuff
#ifdef ARDUINO_ARCH_RP2040
TwoWire* i2cBus = new TwoWire((int)I2C_SDA_PIN, (int)I2C_SCL_PIN);
I2CMultiplexer i2cMultiplexer(i2cBus, I2C_MULTIPLEXER_ADDRESS, I2C_MULTIPLEXER2_ADDRESS);
PowerControl powerCtl(i2cBus, &i2cMultiplexer, POWER_MX_PORT, I2C_POWER_CTL_ADDR);
#endif
#ifdef ARDUINO_ARCH_ESP32
TwoWire* i2cBus = &Wire;
TwoWire* i2cBus2 = &Wire1;
I2CMultiplexer i2cMultiplexer(i2cBus, i2cBus2, I2C_MULTIPLEXER2_ADDRESS);
PowerControl powerCtl(i2cBus, I2C_POWER_CTL_ADDR);
SwitcherooWiFi* switcheroo = new SwitcherooWiFi("growbot-switcheroo");
TimeKeeper* lightsTimekeeper = new TimeKeeper(switcheroo, SWITCHEROO_LIGHTS_PORT, &config.lightSchedule, &data.lightsOn);
TimeKeeper* roomFansTimekeeper = new TimeKeeper(switcheroo, SWITCHEROO_ROOM_FAN_PORT, &config.roomFanSchedule, &data.roomFanOn);
PumpControl* pumpControl = new PumpControl(switcheroo, SWITCHEROO_PUMP_PORT, &data, &config.pumpOn);
#endif

FixedThermostat chillerThermostat;
int operating_mode = GROWBOT_MODE_NORMAL;


void sendState() {
    state.config = config;
    state.data = data;
    state.current_mode = operating_mode;
    if (!dataConn.sendState(state)) {
      dbg.printf("State was not sent!\n");
    }
    dbg.printf("sending state, lights flag is: %d\n", state.data.lightsOn);
}

void doImportantTicks() {
  bool doSendState = false;
  #if ARDUINO_ARCH_ESP32
  ArduinoOTA.handle();
  wifiMgr.handle();
  
  doSendState = doSendState || lightsTimekeeper->handle();
  doSendState = doSendState || roomFansTimekeeper->handle();
  #endif
  powerCtl.handle();
  dataConn.handle();
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

Sensorama sensorama = Sensorama(i2cBus, i2cBus2, &i2cMultiplexer, &data, &config, doImportantTicks);



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

  //set power levels
  powerCtl.setChannelLevel(POWER_EXHAUST_FAN_PORT, config.exhaustFanPercent);
  delay(50);
  powerCtl.setChannelLevel(POWER_INTAKE_FAN_PORT, config.intakeFanPercent);
  delay(50);

  //set schedules
  lightsTimekeeper->handle(true);
  roomFansTimekeeper->handle(true);
 }

#ifdef ARDUINO_ARCH_ESP32
void reset_system()
{
    rtc_wdt_protect_off();      //Disable RTC WDT write protection
    //Set stage 0 to trigger a system reset after 1000ms
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 100);
    rtc_wdt_enable();           //Start the RTC WDT timer
    rtc_wdt_protect_on();       //Enable RTC WDT write protection
}
#endif
#ifdef ARDUINO_ARCH_RP2040
void reset_system() {
  //todo:??????
}
#endif



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
      dbg.println("Entering pH calibration mode");
      operating_mode = GROWBOT_MODE_CALIBRATING_PH_SENSOR;
    } else if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
      dbg.println("Entering EC calibration mode");
      operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR;
    } else if (mode == GROWBOT_MODE_REBOOT) {
      dbg.println("Reset system mode received, resetting now");
        reset_system();
    } else {
      dbg.printf("Invalid mode transition from normal to mode %d\n", mode);
      return;
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID) {
        dbg.println("in ph calibration mode, got message to set the mid point calibration");
        //todo: actual mid point calib
        if (!((PhSensor*)sensorama.getSensor("waterData.ph"))->calibrate(EZO_CALIBRATE_MID, PH_CALIBRATION_MID)) {
          dbg.println("CALIBRATION FAILURE!!!");
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
          dbg.println("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("pH calibration canceled.  calibration is busted now!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from ph calibration mid to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_HIGH) {
        dbg.println("in ph calibration low set mode, got message to set the high point calibration");
        //todo: actual high point calibration here
        //switch back to normal mode from here
        if (!((PhSensor*)sensorama.getSensor("waterData.ph"))->calibrate(EZO_CALIBRATE_HIGH, PH_CALIBRATION_HIGH)) {
          dbg.println("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_NORMAL;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("pH calibration canceled.  calibration is busted now!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from ph calibration low to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY) {
        dbg.println("in ec calibration mode, got message to set the dry point calibration");
        //todo: actual ec dry calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_DRY, 0)) {
          dbg.println("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("EC calibration canceled");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from EC calibration start to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW) {
        dbg.println("in ec calibration mode, got message to set the low point calibration");
        //todo: actual ec low calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_LOW, EC_CALIBRATION_LOW)) {
          dbg.println("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("EC calibration canceled.. ec calibration is hosed!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from EC calibration dry to %d\n", mode);
        return;
      }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW) {
    if (mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_HIGH) {
        dbg.println("in ec calibration mode, got message to set the high point calibration");
        //todo: actual ec high calib
        if (!((ConductivitySensor*)sensorama.getSensor("waterData.tds"))->calibrate(EZO_CALIBRATE_HIGH, EC_CALIBRATION_HIGH)) {
          dbg.println("CALIBRATION FAILURE!!!");
          return;
        }
        operating_mode = GROWBOT_MODE_NORMAL;
      } else if (mode == GROWBOT_MODE_NORMAL) {
        dbg.println("EC calibration canceled.. ec calibration is hosed!");
        operating_mode = GROWBOT_MODE_NORMAL;
      } else {
        dbg.printf("Invalid mode transition from EC calibration low to %d\n", mode);
        return;
      }
  }
  sendState();
}

void updateThermostats() {
  chillerThermostat.handle();
}
#ifdef ARDUINO_ARCH_ESP32
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
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setRebootOnSuccess(false);
  ArduinoOTA
    .onStart([]() {
      dbg.printf("Start updating\n");
    })
    .onEnd([]() {
      dbg.println("Update End, setting watchdog");
      reset_system();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      dbg.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      dbg.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) dbg.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) dbg.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) dbg.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) dbg.println("Receive Failed");
      else if (error == OTA_END_ERROR) dbg.println("End Failed");
    });
    chillerThermostat.init(&config.waterChillerThermostat, &data.controlBucket.temperatureC,
      [](bool isOn, float value) { 
        if (isOn) {
          dbg.printf("chiller thermostat is on \n");
          switcheroo->setPowerToggle(SWITCHEROO_CHILLER_PORT, true);
          data.waterChillerStatus = 100;
        } else {
          dbg.printf("chiller thermostat is off \n");
          switcheroo->setPowerToggle(SWITCHEROO_CHILLER_PORT, false);
          data.waterChillerStatus = 0;
        }
      }
    );
    switcheroo->init();
    dbg.print("Wifi...");
    wifiMgr.init();
    dbg.println("connecting.");
}
#elif defined(PICO_SDK_VERSION_MAJOR)
void setupIO() {
  //stupid garbage implementation of Wire.h, can't even change the pins
  i2cBus->setClock(I2C_FREQ);
  i2cBus->begin();  
}
#endif
void setup() {  
  Serial.begin(115200);
  nvStore.init();
  dbg.println("Growbot v.01 starting up...");
  setupIO();
  delay(3000);
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
  sensorama.update();
  delay(1000);
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
// debug_scan_i2c(i2cBus, &i2cMultiplexer);
//debug_scan_i2c(i2cBus2, &i2cMultiplexer);
      sensorama.update();
      updateThermostats();
      pumpControl->handle();
      sendState();
      dbg.printf("Waiting to tick again for %d\n", config.samplingIntervalMS);
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