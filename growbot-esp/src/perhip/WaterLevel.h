#include <Arduino.h>
#include "I2CMultiplexer.h"
#include "../DebugUtils.h"
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <Ticker.h>

#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define WATERLEVEL_I2C_ADDR 0x59


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
        int readSensorPositionIndex() {
            this->doPlex();
            int readnum = this->_wire->requestFrom(WATERLEVEL_I2C_ADDR, 4);
            if (readnum != 4) {
                dbg.printf("Water level failed to request 4 bytes from sensor, result: %d!\n", readnum);
                return -1;
            }
            //oops, 32u4 is 16 bit ints i guess
            int16_t someBytes[2] = {-1,-1};
            size_t bytesRead = this->_wire->readBytes((byte*)someBytes, 4);
            if (bytesRead != 4) {
                dbg.printf("Water level read %d bytes instead of 4!\n", bytesRead);
                return -1;
            }
            int16_t aRead = someBytes[0];
            
            if (aRead <= 0 || aRead >= 1023) {
                dbg.printf("Water level read invalid value: %d\n", aRead);
                return -1;
            }
            int idx = -1;

            if (aRead > 250) {//empty, usually ~260-273 
                idx = 0;
            } else if (aRead > 200) {
                idx = 1;
            } else if (aRead > 170) {
                idx = 2;
            } else if (aRead > 150) {
                idx = 3;
            } else if (aRead > 130) {
                idx = 4;
            } else if (aRead > 120) {
                idx = 5;
            } else if (aRead > 100) {
                idx = 6;
            } else if (aRead > 90) {
                idx = 7;
            } else if (aRead > 60) {
                idx = 8;
            } else  { //if (aRead > 40) {
                idx = 9;
            }
            return idx;
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
            reading.values[0] = NAN;

            int floatPos = this->readSensorPositionIndex();
            if (floatPos < 0 || floatPos > 9) {
                reading.isSuccessful = false;
                dbg.printf("Water Level: Bad sensor position index %d\n", floatPos);
                return reading;
            }
            switch (floatPos) {
                case 0:
                    reading.isSuccessful = true;
                    reading.values[0] = 0;
                    break;
                case 1:
                    reading.isSuccessful = true;
                    reading.values[0] = 10;
                    break;
                case 2:
                    reading.isSuccessful = true;
                    reading.values[0] = 20;
                    break;
                case 3:
                    reading.isSuccessful = true;
                    reading.values[0] = 30;
                    break;
                case 4:
                    reading.isSuccessful = true;
                    reading.values[0] = 40;
                    break;
                case 5:
                    reading.isSuccessful = true;
                    reading.values[0] = 50;
                    break;
                case 6:
                    reading.isSuccessful = true;
                    reading.values[0] = 60;
                    break;
                case 7:
                    reading.isSuccessful = true;
                    reading.values[0] = 75;
                    break;
                case 8:
                    reading.isSuccessful = true;
                    reading.values[0] = 90;
                    break;
                case 9:
                    reading.isSuccessful = true;
                    reading.values[0] = 100;
                    break;
                default:
                    reading.isSuccessful = false;
                    dbg.printf("Water Level: Bad sensor position index %d\n", floatPos);
                    break;
            }
            
            return reading;
        }
        void finishRead(DeferredReading &reading) {
           //nothing to do here
        }
};



#endif