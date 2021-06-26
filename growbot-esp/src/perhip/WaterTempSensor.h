#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "../DebugUtils.h"
#include "SensorBase.h"
#ifndef WATERTEMPSENSOR_H
#define WATERTEMPSENSOR_H

class WaterTempSensor : public SensorBase {
    public:
        virtual void init();
        virtual float read();
};

class DallasWaterTempSensor : public WaterTempSensor {
    private:
        DeviceAddress addr;
        DallasTemperature* sensor;
        bool isOk = false;
    public:
        DallasWaterTempSensor(DallasTemperature* sensor, std::initializer_list<uint8_t> address) {
            assert(address.size() == 8);
            std::copy(address.begin(), address.end(), this->addr);
            this->sensor = sensor;
        }
        void init() {
            this->sensor->setWaitForConversion(false);
            this->sensor->begin();
            this->isOk = this->sensor->isConnected(this->addr);
            if (!this->isOk) {
                dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            } else {
                if (!this->sensor->setResolution(this->addr, 12)) {
                    dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to set sensor resolution!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                    this->isOk = false;
                }
            }
            this->sensor->requestTemperatures();
        }

        float read() {
            if (!this->isOk) {
                dbg.printf("DallasWaterTempSensor: not on on read(), calling init() again...\n");
                this->init();
            }
            if (!this->isOk) {
                dbg.printf("Tried to read Dallas sensor at address %x %x %x %x %x %x %x %x but it is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                return NAN;
            }
            if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
                dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                this->isOk = false;
                return NAN;
            }
            delay(750);
            for (byte i = 0; i < 3; i++) {
                float val = this->sensor->getTempC(this->addr);
                if (val == -127) {
                    dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature, retry %d\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7], i);
                } else {
                    return val;
                }
            }
            return NAN;
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.isComplete = false;
            reading.isSuccessful = false;
            reading.values[0] = NAN;
            reading.readingCount = 1;
            reading.deferUntil = millis() + 1200;
            if (!this->isOk) {
                this->init();
            }
            if (!this->isOk) {
                dbg.printf("Tried to read Dallas sensor at address %x %x %x %x %x %x %x %x but it is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                reading.isComplete = true;
                reading.isSuccessful = false;
                reading.deferUntil = 0;
            } else if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
                delay(100);
                //I don't know why this takes twice, I think maybe I got some cheap knockoffs
                if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
                    dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x !\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                   // this->isOk = false;
                    reading.isComplete = true;
                    reading.isSuccessful = false;
                    reading.deferUntil = 0;
                }
            }
            return reading;
        }
        void finishRead(DeferredReading &reading) {
            reading.isComplete = true;
            float val = this->sensor->getTempC(this->addr);
            if (val == -127) {
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            } else {
                reading.isSuccessful = true;
                reading.values[0] = val;
            }
        }
};

#endif