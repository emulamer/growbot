#include <Arduino.h>

#include "SensorBase.h"
#include "ReadingNormalizer.h"
#include "GrowbotData.h"
#include "perhip/WaterTempSensor.h"
#include "perhip/LuxSensor.h"
#include "perhip/WSWaterLevel.h"
#include "perhip/I2CWaterLevel.h"
#include "perhip/PhSensor.h"
#include "perhip/ConductivitySensor.h"
#include "perhip/TempHumiditySensor.h"
#include "Config.h"
#include "perhip/I2CMultiplexer.h"
#ifdef ARDUINO_ARCH_RP2040
#include "perhip/PicoOnewireTempSensor.h"
#endif
#ifdef ARDUINO_ARCH_ESP32
#include "perhip/DallasWaterTempSensor.h"
#endif

#ifndef SENSORAMA_H
#define SENSORAMA_H
#define MAX_READ_TIME_MS 5000
#define I2COBJ this->wire
#define I2C2OBJ this->wire2
#define MAKELUX(NAME, MXPORT) new SensorHolder(\
                new Max44009Sensor(I2C2OBJ, i2cMultiplexer, MXPORT),\
                { new ReadingNormalizer(#NAME, 10, 3, -1, 0, 100000) },\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)
            

#define MAKETEMPHUMID(NAME, TYPE, MXPORT) new SensorHolder(\
                new TYPE(I2C2OBJ, i2cMultiplexer, MXPORT),\
                { new ReadingNormalizer(#NAME ".temperatureC", 10, 3, .5f, 0, 70),\
                  new ReadingNormalizer(#NAME ".humidity", 10, 3, .5f, 1, 99) },\
                #NAME,\
                2,\
                { &this->data->NAME.temperatureC, &this->data->NAME.humidity },\
                NULL)
            
#define MAKEWATERLEVEL(NAME, MXPORT) new SensorHolder(\
                new I2CWaterLevel(I2COBJ, i2cMultiplexer, MXPORT),\
                {new ReadingNormalizer(#NAME, 10, 3, .5f, 0, 100)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

#define MAKEWATERTEMP(NAME, TYPE, ADDR) new SensorHolder(\
                new TYPE(ADDR), \
                {new ReadingNormalizer(#NAME, 10, 12, .5f, 0, 70)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

#define MAKEPH(NAME, ENABLEPIN, MXPORT) new SensorHolder(\
                new PhSensor(I2COBJ, this->i2cMultiplexer, MXPORT, ENABLEPIN),\
                {new ReadingNormalizer(#NAME, 10, 3, .3f, 1, 14)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

#define MAKETDS(NAME, ENABLEPIN, MXPORT) new SensorHolder(\
                new ConductivitySensor(I2COBJ, this->i2cMultiplexer, MXPORT, ENABLEPIN),\
                {new ReadingNormalizer(#NAME, 10, 3, .5f, 1, 2000)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)
static bool millisElapsed(unsigned long ms) {
            if (ms <= millis()) {
            return true;
            }
            //todo: need to make sure this handles the 52 day wrap here or things will break
            if (abs(ms - millis()) > 10000000) {
            return true;
            }
            return false;
        }

struct SensorHolder {
    SensorHolder(SensorBase* _sensor, std::initializer_list<ReadingNormalizer*> _normalizers, const char* _name, byte _valueCount, std::initializer_list<float*> _valueAddrs, void* _configAddr) {
        this->sensor = _sensor;
        this->name = _name;
        this->valueCount = _valueCount;
        this->configAddr = _configAddr;
        assert(_valueAddrs.size() <= _valueCount);
        std::copy(_valueAddrs.begin(), _valueAddrs.end(), this->valueAddrs);
        assert(_normalizers.size() <= _valueCount);
        std::copy(_normalizers.begin(), _normalizers.end(), this->normalizers);
    }
    SensorBase* sensor;
    ReadingNormalizer* normalizers[MAX_READING_COUNT];
    const char* name;
    byte valueCount;
    float* valueAddrs[MAX_READING_COUNT];
    void* configAddr;
};

class Sensorama {
    private:
        std::function<void()> importantTicks;
        GrowbotData* data;
        GrowbotConfig* config;
        std::vector<SensorHolder*> sensors;
        I2CMultiplexer* i2cMultiplexer;
        OneWire* oneWire = new OneWire(ONEWIRE_PIN);
        DallasTemperature* dallasTemp = new DallasTemperature(oneWire);        
        TwoWire* wire;
        TwoWire* wire2;
    public:
        Sensorama(TwoWire* wire, TwoWire* wire2, I2CMultiplexer* i2cPlexer, GrowbotData* growbotData, GrowbotConfig* growbotConfig, std::function<void()> importantTicksCallback) {
            this->wire = wire;
            this->wire2 = wire2;
            this->i2cMultiplexer = i2cPlexer;
            this->data = growbotData;    
            this->config = growbotConfig;
            this->importantTicks = importantTicksCallback;            
        }
        void init() {
            //water quality sensors
           this->sensors.push_back(MAKEPH(waterData.ph, PH_ENABLE_PIN, PH_SENSOR_MX_PORT));
           this->sensors.push_back(MAKETDS(waterData.tds, TDS_ENABLE_PIN, TDS_SENSOR_MX_PORT));

           //control bucket sensors
           this->sensors.push_back(MAKEWATERTEMP(controlBucket.temperatureC, DallasWaterTempSensor, WT_CONTROL_BUCKET_ADDRESS));
           this->sensors.push_back(new SensorHolder(new WSWaterLevel("growbot-flow", 8118),
                                                    {new ReadingNormalizer("controlBucket.waterLevelPercent", 10, 3, .5f, 0, 100)},
                                                    "controlBucket.waterLevelPercent", 1,
                                                    { &this->data->controlBucket.waterLevelPercent }, NULL));
           //this->sensors.push_back(MAKEWATERLEVEL(controlBucket.waterLevelPercent, CONTROL_WATER_LEVEL_MX_PORT));

           //light sensors
           this->sensors.push_back(MAKELUX(luxAmbient1, 15));
           //this->sensors.push_back(MAKELUX(luxAmbient2, 14));
         //  this->sensors.push_back(MAKELUX(luxAmbient3, 13));
         //  this->sensors.push_back(MAKELUX(luxAmbient4, 12));

           //temperature/humidity sensors
           this->sensors.push_back(MAKETEMPHUMID(ambientInternal1, BME280Sensor, 15));
          // this->sensors.push_back(MAKETEMPHUMID(intakeInternal, BME280Sensor, 13));
          // this->sensors.push_back(MAKETEMPHUMID(intakeExternal, SHTC3Sensor, 9));
          // this->sensors.push_back(MAKETEMPHUMID(exhaustExternal, SHTC3Sensor, 10));            
          // this->sensors.push_back(MAKETEMPHUMID(ambientInternal1, BME280Sensor, 15));
          // this->sensors.push_back(MAKETEMPHUMID(ambientInternal2, BME280Sensor, 12));
           //this->sensors.push_back(MAKETEMPHUMID(ambientInternal3, BME280Sensor, 13));
          // this->sensors.push_back(MAKETEMPHUMID(lights, SHTC3Sensor, 14));
            configChanged();
            for (auto sensor : this->sensors) sensor->sensor->init();
        }
        void configChanged() {
            for (auto sensor : this->sensors) {
                if (sensor->configAddr != NULL) {
                    sensor->sensor->reconfigure(sensor->configAddr);
                }
            }
        }
        SensorBase* getSensor(const char* name) {
            for (auto sensor : this->sensors) {
                if (strcmp(name, sensor->name) == 0) {
                    return sensor->sensor;
                }
            }
            dbg.wprintf("Sensorama: getSensor(%s) failed!  No sensor with that name!", name);
            return NULL;
        }
        bool updateOne(const char* name, bool normalize = true) {
            bool ok = false;
            for (auto sensor : this->sensors) {
                if (strcmp(name, sensor->name) == 0) {
                    dbg.dprintf("Sensorama: updateOne(%s)...\n", sensor->name);
                    sensor->sensor->preupdate(this->data);
                    DeferredReading reading = sensor->sensor->startRead();
                    unsigned long waitTime = reading.deferUntil - millis();
                    if (waitTime > 0 && waitTime < 5000) {
                        delay(waitTime);
                    }
                    
                    sensor->sensor->finishRead(reading);
                    if (!reading.isSuccessful) {
                        dbg.wprintf("Sensorama: updateOne(%s) failed!  Will use NaN!\n", sensor->name);
                        for (byte x = 0; x < MAX_READING_COUNT; x++) {
                            reading.values[x] = NAN;
                        }
                    } else {
                        dbg.dprintf("Sensorama: updateOne(%s) read succeeded\n", sensor->name);
                        ok = true;
                    }
                    
                    for (byte x = 0; x < sensor->valueCount; x++) {
                        if (normalize) {
                            //dbg.printf("\tNormalizing value #%d of %f to %s\n", x, reading->values[x], sensor->name);
                            *sensor->valueAddrs[x] = sensor->normalizers[x]->normalizeReading(reading.values[x]);
                        } else {
                            *sensor->valueAddrs[x] = reading.values[x];
                        }
                        dbg.dprintf("Sensorama: %s OK\n", sensor->name);
                    }
                    return ok;
                }
            }
            dbg.wprintf("Sensorama: couldn't updateOne because name %s wasn't found!\n", name);
            return false;
        }
        void update() {
            dbg.dprintln("Sensorama: starting update");
            for (auto sensor : this->sensors) sensor->sensor->preupdate(this->data);
            int sensorCount = this->sensors.size();
            DeferredReading* readings = (DeferredReading*)malloc(sensorCount * sizeof(DeferredReading));
            int ctr = 0;
            for (auto sensor : this->sensors) {
                readings[ctr] = sensor->sensor->startRead();
                ctr++;
            }
            
            unsigned long start = millis();
            dbg.dprintf("Sensorama: waiting for readings to complete...\n");

            bool outstanding = false;
            
            do {
                outstanding = false;
                for (int i = 0; i < sensorCount; i++) {
                    if (millis() - start > MAX_READ_TIME_MS) {
                        dbg.eprintf("Sensorama: ERROR!  Read loop timed out!");
                        break;
                    }
                    auto reading = &readings[i];
                    if (!reading->isComplete) {
                        outstanding = true;
                        if (millisElapsed(reading->deferUntil)) {

                            this->sensors[i]->sensor->finishRead(*reading);
                            if (!reading->isSuccessful) {
                                for (byte x = 0; x < MAX_READING_COUNT; x++) {
                                    reading->values[x] = NAN;
                                }
                            }
                            reading->isComplete = true;
                        }
                    }
                }
                this->importantTicks();
                delay(10);
            } while (outstanding);
            for (int i = 0; i < sensorCount; i++) {
                auto reading = &readings[i];
                auto sensor = this->sensors[i];
                for (byte x = 0; x < sensor->valueCount; x++) {
                    //dbg.printf("\tNormalizing value #%d of %f to %s\n", x, reading->values[x], sensor->name);
                    *sensor->valueAddrs[x] = sensor->normalizers[x]->normalizeReading(reading->values[x]);
                }
            }
            for (int i = 0; i < sensorCount; i++) {
                auto reading = &readings[i];
                auto sensor = this->sensors[i];
                if (reading->isSuccessful) {
                     dbg.dprintf("Sensorama: %s OK\n", sensor->name);
                } else {
                    dbg.eprintf("Sensorama: %s FAILED\n", sensor->name);
                }
                delay(10);            
            }
            free(readings);
            unsigned long end = millis();
            dbg.dprintf("Sensorama: finished updating sensors in %lu ms\n", (end - start));
        }
        void updateSync() {
            bool* okays = (bool*)malloc(this->sensors.size());
            dbg.dprintln("Sensorama: starting update");
            unsigned long start = millis();
            int ctr = 0;
            for (auto sensor : this->sensors) {
                okays[ctr] = this->updateOne(sensor->name);
                ctr++;
            }
            unsigned long end = millis();
            dbg.dprintf("Sensorama: finished updating sensors in %lu ms\n", (end - start));
            delay(200);
            for(uint i = 0; i < this->sensors.size(); i++) {
                if (okays[i]) {
                    dbg.dprintf("Sensorama: %s OK\n", this->sensors[i]->name);
                } else {
                    dbg.eprintf("Sensorama: %s FAILED\n", this->sensors[i]->name);
                }
                
            }
            free(okays);
        }
        void handle() {
             for (auto sensor : this->sensors) {
                 sensor->sensor->handle();
             }
        }
};



#endif