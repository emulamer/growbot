#ifdef ARDUINO_ARCH_ESP32
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "../Config.h"
#include "../DebugUtils.h"
#include "SensorBase.h"
#ifndef DALLASWATERTEMPSENSOR_H
#define DALLASWATERTEMPSENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

OneWire* onewire = new OneWire(ONEWIRE_PIN);
DallasTemperature* dallasTemp = new DallasTemperature(onewire);

bool onewireInitted = false;

class DallasWaterTempSensor : public SensorBase {
    private:
        DeviceAddress addr;
        bool isOk = false;
    public:
        DallasWaterTempSensor(std::initializer_list<uint8_t> address) {
            assert(address.size() == 8);
            std::copy(address.begin(), address.end(), this->addr);
        }
        void init() {
            onewire->begin(ONEWIRE_PIN);
            this->isOk = true;
            dallasTemp->setWaitForConversion(false);
            dallasTemp->begin();
            delay(200);
            //dallasTemp->setResolution(12);
            this->isOk = dallasTemp->isConnected(this->addr);
            if (!this->isOk) {
                dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            } 
           // dallasTemp->requestTemperatures();//)(this->addr);
        }

        float read() {


            // if (!this->isOk) {
            //     dbg.printf("DallasWaterTempSensor: not on on read(), calling init() again...\n");
            //     this->init();
            // }
            // if (!this->isOk) {
            //     dbg.printf("Tried to read Dallas sensor at address %x %x %x %x %x %x %x %x but it is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //     return NAN;
            // }
            // if (!this->sensor->requestTemperaturesByAddress(this->addr)) {
            //     dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x is not connected!\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
            //     this->isOk = false;
            //     return NAN;
            // }
            // delay(750);
            // for (byte i = 0; i < 3; i++) {
            //     float val = this->sensor->getTempC(this->addr);
            //     if (val == -127) {
            //         dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature, retry %d\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7], i);
            //     } else {
            //         return val;
            //     }
            // }
            return NAN;
        }
        DeferredReading startRead() {

            // onewire->reset_search();
            // DeviceAddress addr;
            // onewire->begin(ONEWIRE_PIN);
            // dallasTemp->begin();
            // delay(200);
            // dbg.printf("Finding onewire sensors...\n");
            // while (onewire->search(addr)) {
            //     dbg.printf("Onewire device found, address: %0x, %0x, %0x, %0x, %0x, %0x, %0x, %0x\n",addr[0],addr[1],addr[2],addr[3],addr[4],addr[5],addr[6],addr[7]);
            // }
            // dbg.printf("Done finding onewire sensors\n");

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
                this->isOk = false;
            } else  {
                if (!dallasTemp->requestTemperaturesByAddress(this->addr)) {
                    //dallasTemp->requestTemperatures();
                    dbg.println("failed to request water temp sensor one time");
                    delay(100);
                    if (!dallasTemp->requestTemperaturesByAddress(this->addr)) {
                        dbg.println("failed to request water temp sensor a second time");
                    }
                //I don't know why this takes twice, I think maybe I got some cheap knockoffs
                // if (!dallasTemp->requestTemperaturesByAddress(this->addr)) {
                //     dbg.printf("Failed to request temperature from Dallas sensor at address %x %x %x %x %x %x %x %x !\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7]);
                //     this->isOk = false;
                //     reading.isComplete = true;
                //     reading.isSuccessful = false;
                //     reading.deferUntil = 0;
                }
            }
            return reading;
        }
        void finishRead(DeferredReading &reading) {
            reading.isComplete = true;
            float val = dallasTemp->getTempC(this->addr);
            if (val <= -127) {
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                dbg.printf("Dallas sensor at address %x %x %x %x %x %x %x %x failed to read temperature, returned %f\n", this->addr[0], this->addr[1], this->addr[2], this->addr[3], this->addr[4], this->addr[5], this->addr[6], this->addr[7], val);
            } else {
                reading.isSuccessful = true;
                reading.values[0] = val;
            }
        }
};
#endif
#endif