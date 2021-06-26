#include <Arduino.h>
#include <DallasTemperature.h>
#include "SensorBase.h"
#include "ReadingNormalizer.h"
#include "GrowbotData.h"
#include "perhip/WaterTempSensor.h"
#include "perhip/LuxSensor.h"
#include "perhip/WaterLevel.h"
#include "perhip/PhSensor.h"
#include "perhip/ConductivitySensor.h"
#include "perhip/TempHumiditySensor.h"
#include "Config.h"
#include "perhip/I2CMultiplexer.h"

#ifndef SENSORAMA_H
#define SENSORAMA_H
#define MAX_READ_TIME_MS 5000

#define MAKELUX(NAME, MXPORT) new SensorHolder(\
                new Max44009Sensor(&Wire, i2cMultiplexer, MXPORT),\
                { new ReadingNormalizer(#NAME, 10, 3, .5f, 0, 100000) },\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)
            

#define MAKETEMPHUMID(NAME, TYPE, MXPORT) new SensorHolder(\
                new TYPE(&Wire, i2cMultiplexer, MXPORT),\
                { new ReadingNormalizer(#NAME ".temperatureC", 10, 3, .5f, 0, 70),\
                  new ReadingNormalizer(#NAME ".humidity", 10, 3, .5f, 1, 100) },\
                #NAME,\
                2,\
                { &this->data->NAME.temperatureC, &this->data->NAME.humidity },\
                NULL)
            

#define MAKEWATERLEVEL(NAME, MXPORT, CONFIGPROP) new SensorHolder(\
                new WaterLevel(&Wire, i2cMultiplexer, MXPORT),\
                {new ReadingNormalizer(#NAME, 10, 3, .5f, 0, 70)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                &this->config->CONFIGPROP)

#define MAKEWATERTEMP(NAME, TYPE, ADDR) new SensorHolder(\
                new TYPE(this->dallasTemp, ADDR), \
                {new ReadingNormalizer(#NAME, 10, 3, .5f, 0, 70)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

#define MAKEPH(NAME, ENABLEPIN) new SensorHolder(\
                new PhSensor(&Wire, ENABLEPIN),\
                {new ReadingNormalizer(#NAME, 10, 3, .3f, 1, 14)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

#define MAKETDS(NAME, ENABLEPIN) new SensorHolder(\
                new ConductivitySensor(&Wire, ENABLEPIN),\
                {new ReadingNormalizer(#NAME, 10, 3, .5f, 1, 2000)},\
                #NAME,\
                1,\
                { &this->data->NAME },\
                NULL)

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
        I2CMultiplexer* i2cMultiplexer2;
        OneWire* oneWire = new OneWire(ONEWIRE_PIN);
        DallasTemperature* dallasTemp = new DallasTemperature(oneWire);        

        
    public:
        Sensorama(I2CMultiplexer* i2cPlexer,I2CMultiplexer* i2cPlexer2, GrowbotData* growbotData, GrowbotConfig* growbotConfig, std::function<void()> importantTicksCallback) {
            this->i2cMultiplexer = i2cPlexer;
            this->i2cMultiplexer2 = i2cPlexer2;
            this->data = growbotData;    
            this->config = growbotConfig;
            this->importantTicks = importantTicksCallback;            
        }
        void init() {
            //water quality sensors
           // this->sensors.push_back(MAKEPH(waterData.ph, 0, PH_ENABLE_PIN));
           // this->sensors.push_back(MAKETDS(waterData.tds, 0, TDS_ENABLE_PIN));

            //control bucket sensors
           // this->sensors.push_back(MAKEWATERTEMP(controlBucket.temperatureC, DallasWaterTempSensor, WT_CONTROL_BUCKET_ADDRESS));
           // this->sensors.push_back(MAKEWATERLEVEL(controlBucket.waterLevelPercent, 1, controlWaterLevelCalibration));

            //light sensors
            this->sensors.push_back(MAKELUX(luxAmbient1, 7));
           // this->sensors.push_back(MAKELUX(luxAmbient2, 1));
           // this->sensors.push_back(MAKELUX(luxAmbient3, 4));
           // this->sensors.push_back(MAKELUX(luxAmbient4, 5));

            //temperature/humidity sensors
            this->sensors.push_back(MAKETEMPHUMID(exhaustInternal, BME280Sensor, 7));
          //  this->sensors.push_back(MAKETEMPHUMID(intakeInternal, BME280Sensor, 1));
           // this->sensors.push_back(MAKETEMPHUMID(intakeExternal, SHTC3Sensor, 2));
           // this->sensors.push_back(MAKETEMPHUMID(exhaustExternal, SHTC3Sensor, 3));            
          //  this->sensors.push_back(MAKETEMPHUMID(ambientInternal1, BME280Sensor, 4));
          //  this->sensors.push_back(MAKETEMPHUMID(ambientInternal2, BME280Sensor, 5));
          //  this->sensors.push_back(MAKETEMPHUMID(ambientInternal3, BME280Sensor, 6));
           // this->sensors.push_back(MAKETEMPHUMID(lights, SHTC3Sensor, 7));
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
            dbg.printf("Sensorama: getSensor(%s) failed!  No sensor with that name!", name);
            return NULL;
        }
        bool updateOne(const char* name) {
            bool ok = false;
            for (auto sensor : this->sensors) {
                if (strcmp(name, sensor->name) == 0) {
                    dbg.printf("Sensorama: updateOne(%s)...\n", sensor->name);
                    sensor->sensor->preupdate(this->data);
                    DeferredReading reading = sensor->sensor->startRead();
                    unsigned long waitTime = reading.deferUntil - millis();
                    if (waitTime > 0 && waitTime < 5000) {
                        delay(waitTime);
                    }
                    
                    sensor->sensor->finishRead(reading);
                    if (!reading.isSuccessful) {
                        dbg.printf("Sensorama: updateOne(%s) failed!  Will use NaN!\n", sensor->name);
                        for (byte x = 0; x < MAX_READING_COUNT; x++) {
                            reading.values[x] = NAN;
                        }
                    } else {
                        dbg.printf("Sensorama: updateOne(%s) read succeeded\n", sensor->name);
                        ok = true;
                    }
                    
                    for (byte x = 0; x < sensor->valueCount; x++) {
                        //dbg.printf("\tNormalizing value #%d of %f to %s\n", x, reading->values[x], sensor->name);
                        *sensor->valueAddrs[x] = sensor->normalizers[x]->normalizeReading(reading.values[x]);
                        dbg.printf("Sensorama: %s OK\n", sensor->name);
                    }
                    return ok;
                }
            }
            dbg.printf("Sensorama: couldn't updateOne because name %s wasn't found!\n", name);
            return false;
        }
        void update() {
            dbg.println("Sensorama: starting update");
            for (auto sensor : this->sensors) sensor->sensor->preupdate(this->data);
            int sensorCount = this->sensors.size();
            DeferredReading readings[sensorCount];
            int ctr = 0;
            for (auto sensor : this->sensors) {
                readings[ctr] = sensor->sensor->startRead();
                ctr++;
            }
            
            unsigned long start = millis();
            dbg.printf("Sensorama: waiting for readings to complete...\n");

            bool outstanding = false;
            
            do {
                outstanding = false;
                for (int i = 0; i < sensorCount; i++) {
                    if (millis() - start > MAX_READ_TIME_MS) {
                        dbg.printf("Sensorama: ERROR!  Read loop timed out!");
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
                delay(100);
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
                     dbg.printf("Sensorama: %s OK\n", sensor->name);
                } else {
                    dbg.printf("Sensorama: %s FAILED\n", sensor->name);
                }
                delay(100);            
            }
            unsigned long end = millis();
            dbg.printf("Sensorama: finished updating sensors in %lu ms\n", (end - start));
        }
        void updateSync() {
            bool okays[this->sensors.size()];
            dbg.println("Sensorama: starting update");
            unsigned long start = millis();
            int ctr = 0;
            for (auto sensor : this->sensors) {
                okays[ctr] = this->updateOne(sensor->name);
                ctr++;
            }
            unsigned long end = millis();
            dbg.printf("Sensorama: finished updating sensors in %lu ms\n", (end - start));
            delay(200);
            for( int i = 0; i < this->sensors.size(); i++) {
                dbg.printf("Sensorama: %s %s\n", this->sensors[i]->name, okays[i]?"OK":"FAILED");
            }
        }
};



#endif