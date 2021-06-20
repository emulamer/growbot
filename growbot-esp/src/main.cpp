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
#define GROWBOT_MODE_NORMAL 0x0
#define GROWBOT_MODE_CALIBRATING_PH_SENSOR 0x20
#define GROWBOT_MODE_CALIBRATING_TDS_SENSOR 0x21

WifiManager wifiMgr(WIFI_SSID, WIFI_PASSWORD);
MQTTDataConnection dataConn(MQTT_HOST, MQTT_PORT, MQTT_TOPIC, MQTT_CONFIG_TOPIC);

GrowbotConfig config;
GrowbotData data;
GrowbotState state;

// onewire water temp sensor addresses
DeviceAddress wtControlBucketAddress = WT_CONTROL_BUCKET_ADDRESS;
DeviceAddress wtBucketAddresses[NUM_BUCKETS] = {WT_BUCKET_1_ADDRESS, 
                                        WT_BUCKET_2_ADDRESS, 
                                        WT_BUCKET_3_ADDRESS, 
                                        WT_BUCKET_4_ADDRESS};


OneWire oneWire(ONEWIRE_PIN);
DallasTemperature waterTempSensors(&oneWire);

//i2c stuff
I2CMultiplexer i2cMultiplexer(I2C_MULTIPLEXER_ADDRESS);
PowerControl powerCtl(I2C_POWER_CTL_ADDR);

//water level ultrasonic sensors
WaterLevel wlControlBucket = WaterLevel(&i2cMultiplexer, WL_CONTROL_BUCKET_MULTIPLEXER_PORT);
WaterLevel* wlBuckets[NUM_BUCKETS];

//ambient temp/humidity sensors
BME280Sensor thInnerExhaust = BME280Sensor(&i2cMultiplexer, 0);
BME280Sensor thInnerIntake = BME280Sensor(&i2cMultiplexer, 1);
SHTC3Sensor thOuterIntake = SHTC3Sensor(&i2cMultiplexer, 2);
SHTC3Sensor thOuterExhaust = SHTC3Sensor(&i2cMultiplexer, 3);
BME280Sensor thInnerAmbient1 = BME280Sensor(&i2cMultiplexer, 4);
BME280Sensor thInnerAmbient2 = BME280Sensor(&i2cMultiplexer, 5);
SHTC3Sensor thInnerAmbient3 = SHTC3Sensor(&i2cMultiplexer, 6);
BME280Sensor thLights = BME280Sensor(&i2cMultiplexer, 7);

//lux sensors (built into the BME280 modules)
Max44009Sensor luxAmbient1 = Max44009Sensor(&i2cMultiplexer, 4);
Max44009Sensor luxAmbient2 = Max44009Sensor(&i2cMultiplexer, 5);
Max44009Sensor luxAmbient3 = Max44009Sensor(&i2cMultiplexer, 6);
Max44009Sensor luxAmbient4 = Max44009Sensor(&i2cMultiplexer, 7);
 
PhSensor phSensor = PhSensor();
ConductivitySensor tdsSensor = ConductivitySensor();

int operating_mode = GROWBOT_MODE_NORMAL;

void updateFromConfig() {
  wlControlBucket.setCalibration(config.controlWaterLevelCalibration);
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    wlBuckets[i]->setCalibration(config.bucketWaterLevelCalibration[i]);
  }

  //set power calibration
  powerCtl.setPowerCalibration(POWER_EXHAUST_FAN_PORT, config.exhaustFanCalibration, false);
  powerCtl.setPowerCalibration(POWER_INTAKE_FAN_PORT, config.intakeFanCalibration, false);
  powerCtl.setPowerCalibration(POWER_PUMP_PORT, config.pumpCalibration, false);
  
  //set power levels
  powerCtl.setChannelLevel(POWER_EXHAUST_FAN_PORT, config.exhaustFanPercent);
  powerCtl.setChannelLevel(POWER_INTAKE_FAN_PORT, config.intakeFanPercent);
  powerCtl.setChannelLevel(POWER_PUMP_PORT, config.pumpPercent);

  //set on/off toggles
  powerCtl.setPowerToggle(POWER_EXHAUST_FAN_PORT, config.exhaustFanOn);
  powerCtl.setPowerToggle(POWER_INTAKE_FAN_PORT, config.intakeFanOn);
  powerCtl.setPowerToggle(POWER_PUMP_PORT, config.pumpOn);
}

void loadConfig() {
  byte version = EEPROM.read(0);
  if (version != CONFIG_VERSION) {
    Serial.print("Config stored in eeprom is old version ");
    Serial.println(version);
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
  Serial.println("saved config");
}

void sendState() {
    state.config = config;
    state.data = data;
    state.current_mode = operating_mode;
    Serial.print("sending state...");
    if (!dataConn.sendState(state)) {
      Serial.println("not sent!");
    } else {
      Serial.println("sent");
    }
}

void onNewConfig(GrowbotConfig &newConfig) {
  config = newConfig;
  saveConfig();
  updateFromConfig();
  sendState();
}

void onModeChange(byte mode) {
  if (mode != GROWBOT_MODE_NORMAL && mode != GROWBOT_MODE_CALIBRATING_PH_SENSOR && mode != GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
    Serial.print("Invalid mode specified: ");
    Serial.println(mode);
    return;
  }
  operating_mode = mode;
}

void initSensors() {
  //start up I2C stuff
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  wlBuckets[0] = new WaterLevel(&i2cMultiplexer, WL_BUCKET_ONE_MULTIPLEXER_PORT);
  wlBuckets[1] = new WaterLevel(&i2cMultiplexer, WL_BUCKET_TWO_MULTIPLEXER_PORT);
  wlBuckets[2] = new WaterLevel(&i2cMultiplexer, WL_BUCKET_THREE_MULTIPLEXER_PORT);
  wlBuckets[3] = new WaterLevel(&i2cMultiplexer, WL_BUCKET_FOUR_MULTIPLEXER_PORT);

  //update various stuff from the current config
  updateFromConfig();

  //setup water temperature
  waterTempSensors.begin();
  waterTempSensors.setResolution(wtControlBucketAddress, 9);
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    waterTempSensors.setResolution(wtBucketAddresses[i], 9);
  }

  //init air temp/humidity sensors
  thInnerExhaust.init();
  thOuterIntake.init();
  thInnerIntake.init();
  thInnerAmbient1.init();
  thInnerAmbient2.init();
  thInnerAmbient3.init();
  thLights.init();

  //init light sensors built into the BME280 modules
  luxAmbient1.init();
  luxAmbient2.init();
  luxAmbient3.init();
  luxAmbient4.init();

  //init ph and tds sensors
  phSensor.init();
  tdsSensor.init();
}

void readWaterTempSensors() {
  data.controlBucket.temperatureC = waterTempSensors.getTempC(wtControlBucketAddress);
  if (data.controlBucket.temperatureC == -127) {
    data.controlBucket.temperatureC = NAN;
  }
  for (byte i = 0; i < NUM_BUCKETS; i++) {
    data.buckets[i].temperatureC = waterTempSensors.getTempC(wtBucketAddresses[i]);
    if (data.buckets[i].temperatureC == -127) {
      data.buckets[i].temperatureC = NAN;
    }
  }
}

void readWaterLevelSensors() {
  data.controlBucket.waterLevelPercent = wlControlBucket.readLevelPercent();
  //TODO: disabled for now, it's slow
  // for (byte i = 0; i < NUM_BUCKETS; i++) {
  //   data.buckets[i].waterLevelPercent = wlBuckets[i]->readLevelPercent();
  // }
}

void readTempHumiditySensors() {
  thInnerExhaust.read(data.exhaustInternal);
  thInnerIntake.read(data.intakeInternal);
  thOuterIntake.read(data.intakeExternal);
  thOuterExhaust.read(data.exhaustExternal);
  thInnerAmbient1.read(data.ambientInternal1);
  thInnerAmbient2.read(data.ambientInternal2);
  thInnerAmbient3.read(data.ambientInternal3);
  thLights.read(data.lights);
}

void readLuxSensors() {
  data.luxAmbient1 = luxAmbient1.readLux();
  data.luxAmbient2 = luxAmbient2.readLux();
  data.luxAmbient3 = luxAmbient3.readLux();
  data.luxAmbient4 = luxAmbient4.readLux();
}

void readWaterQualitySensors(bool ph, bool tds) {
  if (!isnan(data.controlBucket.temperatureC)) {
    Serial.println("notnan");
    if (tds) {
      if (!tdsSensor.read(data.controlBucket.temperatureC, data.waterData)) {
        Serial.println("conductivity sensor failed to read!");
      }
    }
    if (ph) {
      if (!phSensor.read(data.controlBucket.temperatureC, data.waterData)) {
        Serial.println("ph sensor failed to read!");
      }
    }
  } else {
    Serial.println("warning: conductivity sensor being read without temp!");
    if (tds) {
      Serial.println("reading tds");
      if (!tdsSensor.read(data.waterData)) {
        Serial.println("conductivity sensor failed to read!");
      }
    }
    if (ph) {
      Serial.println("reading ph");
      if (!phSensor.read(data.waterData)) {
        Serial.println("ph sensor failed to read!");
      }
    }
  }
}

void readAllSensors() {
  Serial.print("Reading all sensors...");
  readTempHumiditySensors();
  readWaterLevelSensors();
  readWaterTempSensors();
  readWaterQualitySensors(true, true);
  readLuxSensors();
  Serial.println("done");
}

void setup() {
  EEPROM.begin(512);
  Serial.begin(9600);
  Serial.println("Growbot v.01 starting up...");
  ArduinoOTA.setPort(3232);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });


  Serial.print("Loading config...");
  loadConfig();
  Serial.println("loaded.");

  Serial.print("Sensors...");
  initSensors();
  Serial.println("initialized.");

  Serial.print("Wifi...");
  wifiMgr.init();
  Serial.println("connecting.");

  Serial.print("Starting data connection...");
  dataConn.onModeChange(onModeChange);
  dataConn.onNewConfig(onNewConfig);
  dataConn.init();
  Serial.println("started.");
//  Serial2.begin(9600);
}
TempHumidity th;

void loop() {
  ArduinoOTA.handle();
  // thInnerIntake.read(th);
  // Serial.println("inner intake (1)");
  // Serial.println(th.temperatureC);
  // Serial.println(th.humidity);
  // thInnerAmbient1.read(th);
  // Serial.println("inner ambient (4)");
  // Serial.println(th.temperatureC);
  // Serial.println(th.humidity);
  // delay(1000);
  // return;
  // debug_find_onewire_sensors(oneWire);
  // delay(1000);
  // return;
  // uint8_t b;
  // Serial2.write((uint8_t)0x55);
  // Serial2.readBytes(&b, 1);
  // printHex(b);
  // delay(500);
  // Serial2.readBytes(&b, 1);
  // printHex(b);
  // return;
  // debug_scan_i2c();
  // delay(2000);
  // return;
  // wlControlBucket.readLevelPercent();
  // return;
  // Wire.beginTransmission(0x57);
  // Wire.write(1);
  // Wire.endTransmission();
  // delay(120);
  // Wire.requestFrom(0x57, 3);
  // int i = 0;
  // while (Wire.available()) {
  //   distBytes[i++] = Wire.read();
  // }
  // unsigned long distance;
  // distance = (unsigned long)(distBytes[0]) * 65536;
  //   distance = distance + (unsigned long)(distBytes[1]) * 256;
  //   distance = (distance + (unsigned long)(distBytes[2])) / 10000;
  //   Serial.println(distance);

  // return;
    if (operating_mode == GROWBOT_MODE_NORMAL) {
      readAllSensors();
      sendState();
      Serial.print("Waiting to sample again for ");
      Serial.println(config.samplingIntervalMS);
    if (config.samplingIntervalMS < 1) {
      delay(1000);
    } else {        
      delay(config.samplingIntervalMS);
    }
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_PH_SENSOR) {
    readWaterQualitySensors(true, false);
    sendState();
  } else if (operating_mode == GROWBOT_MODE_CALIBRATING_TDS_SENSOR) {
    readWaterQualitySensors(false, true);
    sendState();
  }
}