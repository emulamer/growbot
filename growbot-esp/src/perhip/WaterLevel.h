#include <Arduino.h>
#include "I2CMultiplexer.h"
#include "../DebugUtils.h"
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <Ticker.h>

#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define WATERLEVEL_I2C_ADDR 0x59

#define MIN_VAL 604
#define MAX_VAL 804

class WaterLevel : public SensorBase
{
    private:
        TwoWire* _wire;
        I2CMultiplexer* plexer;
        int busNum;
        bool isMultiplexer;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        }
    public:
        WaterLevel(TwoWire* wire) {
            this->_wire = wire;
            this->isMultiplexer = false;
        }
        WaterLevel(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus) {
            this->_wire = wire;
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        void init() {

        }
        void reconfigure(void* configAddr) {
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.isComplete = true;
            reading.readingCount = 1;
            reading.deferUntil = 0;
            this->doPlex();
            int readnum = this->_wire->requestFrom(WATERLEVEL_I2C_ADDR, 4);
            if (readnum != 4) {
                dbg.printf("Water level failed to request 4 bytes from sensor, result: %d!\n", readnum);
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            }
            //oops, 32u4 is 16 bit ints i guess
            uint32_t val = 0;
            size_t bytesRead = this->_wire->readBytes((byte*)&val, 4);
            if (bytesRead != 4) {
                dbg.printf("Water level read %d bytes instead of 4!\n", bytesRead);
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            }
            dbg.printf("WATERLEVEL READ VAL %d\n", val);
            
            //sanity check
            if (val < 400 || val > 1000) {
                dbg.printf("Water level read invalid value: %d\n", val);
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            } else if (val <= MIN_VAL) {
                dbg.printf("Water level read value under min %d: %d\n", MIN_VAL, val);
                reading.values[0] = 100;
            } else if (val >= MAX_VAL) {
                dbg.printf("Water level read value over max %d: %d\n", MAX_VAL, val);
                reading.values[0] = 0;
            } else {
                float range = MAX_VAL - MIN_VAL;
                float pctEmpty = val;
                pctEmpty = 100 - (((pctEmpty - MIN_VAL)/range)*100);
                reading.values[0] = pctEmpty;
            }
            reading.isSuccessful = true;
            return reading;
        }
        void finishRead(DeferredReading &reading) {
           //nothing to do here
        }
};



#endif