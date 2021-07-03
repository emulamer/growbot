#include <Arduino.h>
#include "I2CMultiplexer.h"
#include "../DebugUtils.h"
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <Ticker.h>

#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define ULTRASONIC_I2C_ADDR 0x59
#define ULTRASONIC_NUM_SAMPLES 5
#define ULTRASONIC_MIN_VALID 1
#define ULTRASONIC_MAX_VALID 500
#define ULTRASONIC_READ_DELAY_MS 120


class WaterLevel : public SensorBase
{
    private:
        TwoWire* _wire;
        
        int m_emptyCm, m_fullCm;
        int m_range;
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
            this->m_fullCm = 1;
            this->m_emptyCm = 100;
            this->m_range = 100;
            this->isMultiplexer = false;
        }
        WaterLevel(TwoWire* wire, WaterLevelCalibration calibration) {
            this->_wire = wire;
            this->isMultiplexer = false;
            this->setCalibration(calibration);
        }
        WaterLevel(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus) {
            this->_wire = wire;
            this->m_fullCm = 1;
            this->m_emptyCm = 100;
            this->m_range = 100;
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        WaterLevel(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus, WaterLevelCalibration calibration) {
            this->setCalibration(calibration);
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        void init() {

        }
        void reconfigure(void* configAddr) {
            setCalibration(*((WaterLevelCalibration*)configAddr));
        }

        void setCalibration(WaterLevelCalibration calibration) {
            this->m_fullCm = calibration.fullCm;
            this->m_emptyCm = calibration.emptyCm;
            this->m_range = calibration.emptyCm - calibration.fullCm;
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.isComplete = false;
            reading.isSuccessful = false;
            reading.readingCount = 1;
            reading.values[0] = NAN;

            this->doPlex();
            delay(50);
            this->_wire->beginTransmission(ULTRASONIC_I2C_ADDR);
            this->_wire->write(5);
            dbg.printf("water level read end: %d\n", this->_wire->endTransmission() );
            reading.deferUntil = millis() + 1000;

            return reading;
        }
        void finishRead(DeferredReading &reading) {
            this->doPlex();
            int readnum = this->_wire->requestFrom(ULTRASONIC_I2C_ADDR, 4);
            if (readnum != 4) {
                dbg.printf("WaterLevel: Failed to read 4 bytes from sensor, got %d!\n", readnum);
                reading.isComplete = true;
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return;
            }
            uint8_t distBytes[4];
            distBytes[0] = 0;
            distBytes[1] = 0;
            distBytes[2] = 0;
            distBytes[3] = 0;
            int i = 0;
            while (this->_wire->available()) {
                int val = this->_wire->read();
                dbg.printf("read byte: %d\n", val);
                distBytes[i++] = val;
            }
            float distance = *((float*)distBytes);

         dbg.printf("read cm distance of %f\n", distance);
            if (distance < ULTRASONIC_MIN_VALID || distance > ULTRASONIC_MAX_VALID || isnan(distance)) {
                dbg.printf("WaterLevel: bad number read from sensor: %f\n", distance);
                reading.values[0] = NAN;
                reading.isSuccessful = false;
                reading.isComplete = false;
            } else {
                auto cmDist = distance;
                if (cmDist == NAN) {
                    reading.values[0] = NAN;
                    reading.isSuccessful = false;
                } else if (cmDist < (unsigned long)this->m_fullCm) {
                    reading.values[0] = 100;
                    reading.isSuccessful = true;
                } else if (cmDist > (unsigned long)this->m_emptyCm) {
                    reading.values[0] = 0;
                    reading.isSuccessful = true;
                } else {
                    float offs = cmDist - this->m_fullCm;
                    float pct = 100 - ((offs* 100) / (float)(this->m_range));
                    reading.values[0] = pct;
                    reading.isSuccessful = true;
                }
            }
            reading.isComplete = true;
        }
};



#endif