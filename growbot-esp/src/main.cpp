#include <Arduino.h>
#include "WaterLevel.h"
#include <Wire.h>
#include <DHT.h>
#include "PowerControl.h"
#include "GrowbotData.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "EEPROMAnything.h"
#include <EEPROM.h>
#include "TempHumiditySensor.h"
#include "DataConnection.h"
#include "PhSensor.h"
#include "ConductivitySensor.h"
#include "WifiManager.h"
#include "Config.h" //all the config stuff is in here
#include "LuxSensor.h"
#include "DebugUtils.h"
#include <ArduinoOTA.h>

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

WifiManager wifiMgr(WIFI_SSID, WIFI_PASSWORD);
MQTTDataConnection dataConn(MQTT_HOST, MQTT_PORT, MQTT_TOPIC, MQTT_CONFIG_TOPIC);

GrowbotConfig config;
GrowbotData data;
GrowbotState state;

// onewire water temp sensor addresses
DeviceAddress wtControlBucketAddress = WT_CONTROL_BUCKET_ADDRESS;

#if NUM_BUCKETS > 0
DeviceAddress wtBucketAddresses[NUM_BUCKETS] = { WT_BUCKET_ADDRESSES };
#endif


OneWire oneWire(ONEWIRE_PIN);
DallasTemperature waterTempSensors(&oneWire);

//i2c stuff
I2CMultiplexer i2cMultiplexer(I2C_MULTIPLEXER_ADDRESS);
PowerControl powerCtl(I2C_POWER_CTL_ADDR);

//water level ultrasonic sensors
WaterLevel wlControlBucket = WaterLevel(&i2cMultiplexer, WL_CONTROL_BUCKET_MULTIPLEXER_PORT);

#if NUM_BUCKETS > 0
WaterLevel* wlBuckets[NUM_BUCKETS];
#endif

//ambient temp/humidity sensors
#ifdef INNER_EXHAUST_TYPE
INNER_EXHAUST_TYPE thInnerExhaust = INNER_EXHAUST_TYPE(&i2cMultiplexer, INNER_EXHAUST_MX_PORT);
#endif
#ifdef INNER_INTAKE_TYPE
INNER_INTAKE_TYPE thInnerIntake = INNER_INTAKE_TYPE(&i2cMultiplexer, INNER_INTAKE_MX_PORT);
#endif
#ifdef OUTER_INTAKE_TYPE
OUTER_INTAKE_TYPE thOuterIntake = OUTER_INTAKE_TYPE(&i2cMultiplexer, OUTER_INTAKE_MX_PORT);
#endif
#ifdef OUTER_EXHAUST_TYPE
OUTER_EXHAUST_TYPE thOuterExhaust = OUTER_EXHAUST_TYPE(&i2cMultiplexer, OUTER_INTAKE_MX_PORT);
#endif
#ifdef INNER_AMBIENT_1_TYPE
INNER_AMBIENT_1_TYPE thInnerAmbient1 = INNER_AMBIENT_1_TYPE(&i2cMultiplexer, INNER_AMBIENT_1_MX_PORT);
#endif
#ifdef INNER_AMBIENT_2_TYPE
INNER_AMBIENT_2_TYPE thInnerAmbient2 = INNER_AMBIENT_2_TYPE(&i2cMultiplexer, INNER_AMBIENT_2_MX_PORT);
#endif
#ifdef INNER_AMBIENT_3_TYPE
INNER_AMBIENT_3_TYPE thInnerAmbient3 = INNER_AMBIENT_3_TYPE(&i2cMultiplexer, INNER_AMBIENT_3_MX_PORT);
#endif
#ifdef LIGHTS_TEMP_TYPE
LIGHTS_TEMP_TYPE thLights = LIGHTS_TEMP_TYPE(&i2cMultiplexer, INNER_LIGHTS_MX_PORT);
#endif

//lux sensors (built into the BME280 modules)
#ifdef LUX_SENSOR_1_MX_PORT
Max44009Sensor luxAmbient1 = Max44009Sensor(&i2cMultiplexer, LUX_SENSOR_1_MX_PORT);
#endif

#ifdef LUX_SENSOR_2_MX_PORT
Max44009Sensor luxAmbient2 = Max44009Sensor(&i2cMultiplexer, LUX_SENSOR_2_MX_PORT);
#endif

#ifdef LUX_SENSOR_3_MX_PORT
Max44009Sensor luxAmbient3 = Max44009Sensor(&i2cMultiplexer, LUX_SENSOR_3_MX_PORT);
#endif

#ifdef LUX_SENSOR_4_MX_PORT
Max44009Sensor luxAmbient4 = Max44009Sensor(&i2cMultiplexer, LUX_SENSOR_4_MX_PORT);
#endif

#ifdef LUX_SENSOR_5_MX_PORT
Max44009Sensor luxAmbient5 = Max44009Sensor(&i2cMultiplexer, LUX_SENSOR_5_MX_PORT);
#endif
 
PhSensor phSensor = PhSensor();
ConductivitySensor tdsSensor = ConductivitySensor();

int operating_mode = GROWBOT_MODE_NORMAL;

void updateFromConfig() {
  wlControlBucket.setCalibration(config.controlWaterLevelCalibration);
  #if NUM_BUCKETS > 0
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    wlBuckets[i]->setCalibration(config.bucketWaterLevelCalibration[i]);
  }
  #endif

  //set power calibration
  powerCtl.setPowerCalibration(POWER_EXHAUST_FAN_PORT, config.exhaustFanCalibration, false);
  powerCtl.setPowerCalibration(POWER_INTAKE_FAN_PORT, config.intakeFanCalibration, false);
  powerCtl.setPowerCalibration(POWER_PUMP_PORT, config.pumpCalibration, false);

  //set on/off toggles
  powerCtl.setPowerToggle(POWER_EXHAUST_FAN_PORT, config.exhaustFanOn);
  powerCtl.setPowerToggle(POWER_INTAKE_FAN_PORT, config.intakeFanOn);
  powerCtl.setPowerToggle(POWER_PUMP_PORT, config.pumpOn);

  //set power levels
  powerCtl.setChannelLevel(POWER_EXHAUST_FAN_PORT, config.exhaustFanPercent);
  powerCtl.setChannelLevel(POWER_INTAKE_FAN_PORT, config.intakeFanPercent);
  powerCtl.setChannelLevel(POWER_PUMP_PORT, config.pumpPercent);
}

void loadConfig() {
  byte version = EEPROM.read(0);
  if (version != CONFIG_VERSION) {
    dbg.print("Config stored in eeprom is old version ");
    dbg.println(version);
    config.exhaustFanPercent = 100;
    config.intakeFanPercent = 100;
    config.pumpPercent = 100;
    config.controlWaterLevelCalibration.emptyCm = 14;
    config.controlWaterLevelCalibration.fullCm = 4;
    for (byte i = 0; i < NUM_BUCKETS; i++) {
      config.bucketWaterLevelCalibration[i].emptyCm = 14;
      config.bucketWaterLevelCalibration[i].fullCm = 4;
    }
    config.exhaustFanCalibration.maxOffset = 30;
    config.exhaustFanCalibration.minOffset = 1;
    config.intakeFanCalibration.maxOffset = 30;
    config.intakeFanCalibration.minOffset = 1;
    config.pumpCalibration.maxOffset = 30;
    config.pumpCalibration.minOffset = 1;
    config.samplingIntervalMS = 10000;
    config.pumpOn = true;
    config.exhaustFanOn = true;
    config.intakeFanOn = true;
    config.overheadLightsOn = true;
    config.pumpOn = true;
    config.sideLightsOn = true;
  } else {
    EEPROM_readAnything<GrowbotConfig>(1, config);
  }
}

void saveConfig() {
  EEPROM.write(0, (byte)CONFIG_VERSION);
  EEPROM_writeAnything<GrowbotConfig>(1, config);
  EEPROM.commit();
  dbg.println("saved config");
}

void sendState() {
    state.config = config;
    state.data = data;
    state.current_mode = operating_mode;
    dbg.print("sending state...");
    if (!dataConn.sendState(state)) {
      dbg.println("not sent!");
    } else {
      dbg.println("sent");
    }
}

void onNewConfig(GrowbotConfig &newConfig) {
  config = newConfig;
  saveConfig();
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
    } else {
      dbg.printf("Invalid mode transition from normal to mode %d\n", mode);
      return;
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR) {
      if (mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID) {
        dbg.println("in ph calibration mode, got message to set the mid point calibration");
        //todo: actual mid point calib
        if (!phSensor.calibrate(EZO_CALIBRATE_MID, PH_CALIBRATION_MID)) {
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
        if (!phSensor.calibrate(EZO_CALIBRATE_LOW, PH_CALIBRATION_LOW)) {
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
        if (!phSensor.calibrate(EZO_CALIBRATE_HIGH, PH_CALIBRATION_HIGH)) {
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
        if (!tdsSensor.calibrate(EZO_CALIBRATE_DRY, 0)) {
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
        if (!tdsSensor.calibrate(EZO_CALIBRATE_LOW, EC_CALIBRATION_LOW)) {
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
        if (!tdsSensor.calibrate(EZO_CALIBRATE_HIGH, EC_CALIBRATION_HIGH)) {
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

void initSensors() {
  //start up I2C stuff
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  #if NUM_BUCKETS > 0
    for (byte i = 0; i < NUM_BUCKETS; i++) {
      wlBuckets[i] = new WaterLevel(&i2cMultiplexer, WL_BUCKET_MULTIPLEXER_PORTS[i]);
    }
  #endif

  //update various stuff from the current config
  updateFromConfig();

  //setup water temperature
  waterTempSensors.begin();
  waterTempSensors.setResolution(wtControlBucketAddress, 9);
  #if NUM_BUCKETS > 0
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    waterTempSensors.setResolution(wtBucketAddresses[i], 9);
  }
  #endif

  //init air temp/humidity sensors
  #ifdef INNER_EXHAUST_TYPE
  thInnerExhaust.init();
  #endif
  #ifdef INNER_INTAKE_TYPE
  thInnerIntake.init();
  #endif
  #ifdef OUTER_INTAKE_TYPE
  thOuterIntake.init();
  #endif
  #ifdef OUTER_EXHAUST_TYPE
  thOuterExhaust.init();
  #endif
  #ifdef INNER_AMBIENT_1_TYPE
  thInnerAmbient1.init();
  #endif
  #ifdef INNER_AMBIENT_2_TYPE
  thInnerAmbient2.init();
  #endif
  #ifdef INNER_AMBIENT_3_TYPE
  thInnerAmbient3.init();
  #endif
  #ifdef LIGHTS_TEMP_TYPE
  thLights.init();
  #endif
  
  //init light sensors built into the BME280 modules
  #ifdef LUX_SENSOR_1_MX_PORT
  luxAmbient1.init();
  #endif
  #ifdef LUX_SENSOR_2_MX_PORT
  luxAmbient2.init();
  #endif
  #ifdef LUX_SENSOR_3_MX_PORT 
  luxAmbient3.init();
  #endif
  #ifdef LUX_SENSOR_4_MX_PORT 
  luxAmbient4.init();
  #endif
  #ifdef LUX_SENSOR_5_MX_PORT 
  luxAmbient5.init();
  #endif  
  
  //init ph and tds sensors
  phSensor.init();
  tdsSensor.init();
}

void readWaterTempSensors() {
  for (byte i = 0; i < 4; i++) {
    data.controlBucket.temperatureC = waterTempSensors.getTempC(wtControlBucketAddress);
    if (data.controlBucket.temperatureC > 0 && data.controlBucket.temperatureC < 70) {
      break;
    }
    dbg.printf("control bucket temp sensor, retrying #%d\n", i);
  }
  if (!(data.controlBucket.temperatureC > 0 && data.controlBucket.temperatureC < 70)) {
    dbg.println("control bucket temp sensor failed!");
    data.controlBucket.temperatureC = NAN;
  }
  
  #if NUM_BUCKETS > 0
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    data.buckets[i].temperatureC = waterTempSensors.getTempC(wtBucketAddresses[i]);
    if (data.buckets[i].temperatureC == -127) {
      data.buckets[i].temperatureC = NAN;
    }
  }
  #endif
}

void readWaterLevelSensors() {
  data.controlBucket.waterLevelPercent = wlControlBucket.readLevelPercent();
  #if NUM_BUCKETS > 0
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    data.buckets[i].waterLevelPercent = wlBuckets[i]->readLevelPercent();
  }
  #endif
}

void readTempHumiditySensors() {
  #ifdef INNER_EXHAUST_TYPE
  if (!thInnerExhaust.read(data.exhaustInternal)) {
    dbg.println("Failed to read inner exhaust sensor!");
  }
  #endif
  #ifdef INNER_INTAKE_TYPE
  if (!thInnerIntake.read(data.intakeInternal)) {
    dbg.println("Failed to read inner intake sensor!");
  }
  #endif
  #ifdef OUTER_INTAKE_TYPE
  if (!thOuterIntake.read(data.intakeExternal)) {
    dbg.println("Failed to read outer intake sensor!");
  }
  #endif
  #ifdef OUTER_EXHAUST_TYPE
  if (!thOuterExhaust.read(data.exhaustExternal)) {
    dbg.println("Failed to read outer exhaust sensor!");
  }
  #endif
  #ifdef INNER_AMBIENT_1_TYPE
  if (!thInnerAmbient1.read(data.ambientInternal1)) {
    dbg.println("Failed to read ambient 1 sensor!");
  }
  #endif
  #ifdef INNER_AMBIENT_2_TYPE
  if (!thInnerAmbient2.read(data.ambientInternal2)) {
    dbg.println("Failed to read ambient 2 sensor!");
  }
  #endif
  #ifdef INNER_AMBIENT_3_TYPE
  if (!thInnerAmbient3.read(data.ambientInternal3)) {
    dbg.println("Failed to read ambient 3 sensor!");
  }
  #endif
  #ifdef LIGHTS_TEMP_TYPE
  if (!thLights.read(data.lights)) {
    dbg.println("Failed to read lights temp sensor!");
  }
  #endif  
}

void readLuxSensors() {
  #ifdef LUX_SENSOR_1_MX_PORT
  data.luxAmbient1 = luxAmbient1.readLux();
  #endif
  #ifdef LUX_SENSOR_2_MX_PORT
  data.luxAmbient2 = luxAmbient2.readLux();
  #endif
  #ifdef LUX_SENSOR_3_MX_PORT
  data.luxAmbient3 = luxAmbient3.readLux();
  #endif
  #ifdef LUX_SENSOR_4_MX_PORT
  data.luxAmbient4 = luxAmbient4.readLux();
  #endif
  #ifdef LUX_SENSOR_5_MX_PORT
  data.luxAmbient5 = luxAmbient5.readLux();
  #endif
}

void readWaterQualitySensors(bool ph, bool tds) {
  if (!isnan(data.controlBucket.temperatureC)) {
    dbg.println("Using control bucket temp for compensation");
    if (tds) {
      if (!tdsSensor.read(data.controlBucket.temperatureC, data.waterData)) {
        dbg.println("conductivity sensor failed to read!");
      }
    }
    if (ph) {
      if (!phSensor.read(data.controlBucket.temperatureC, data.waterData)) {
        dbg.println("ph sensor failed to read!");
      }
    }
  } else {
    dbg.println("warning: conductivity sensor being read without temp!");
    if (tds) {
      dbg.println("reading tds");
      if (!tdsSensor.read(data.waterData)) {
        dbg.println("conductivity sensor failed to read!");
      }
    }
    if (ph) {
      dbg.println("reading ph");
      if (!phSensor.read(data.waterData)) {
        dbg.println("ph sensor failed to read!");
      }
    }
  }
}

void readAllSensors() {
  dbg.print("Reading all sensors...");
  readTempHumiditySensors();
  readWaterLevelSensors();
  readWaterTempSensors();
  readWaterQualitySensors(true, true);
  readLuxSensors();
  dbg.println("done");
}

void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);
  dbg.println("Growbot v.01 starting up...");
  ArduinoOTA.setPort(3232);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      dbg.println("Start updating " + type);
    })
    .onEnd([]() {
      dbg.println("\nEnd");
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



  dbg.print("Loading config...");
  loadConfig();
  dbg.println("loaded.");

  dbg.print("Sensors...");
  initSensors();
  dbg.println("initialized.");

  dbg.print("Wifi...");
  wifiMgr.init();
  dbg.println("connecting.");

  dbg.print("Starting data connection...");
  dataConn.onModeChange(onModeChange);
  dataConn.onNewConfig(onNewConfig);
  dataConn.init();
  dbg.println("started.");
//  Serial2.begin(9600);
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
  ArduinoOTA.handle();

  if (operating_mode == GROWBOT_MODE_NORMAL) {
    if (tickNow()) {
      readAllSensors();
      sendState();
      dbg.printf("Waiting to sample again for %d\n", config.samplingIntervalMS);
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_MID ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_LOW ||
             operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR_SET_HIGH) {
    readWaterQualitySensors(true, false);
    sendState();
    delay(2000);
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_DRY ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_LOW ||
             operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR_SET_HIGH ) {
    readWaterQualitySensors(false, true);
    sendState();
    delay(2000);
  }
}