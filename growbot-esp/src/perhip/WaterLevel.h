#include <Arduino.h>
#include "I2CMultiplexer.h"
#include "../DebugUtils.h"
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <Ticker.h>

#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define ULTRASONIC_I2C_ADDR 0x57
#define ULTRASONIC_NUM_SAMPLES 5
#define ULTRASONIC_MIN_VALID 1
#define ULTRASONIC_MAX_VALID 500
#define ULTRASONIC_READ_DELAY_MS 120


class WaterLevel : public SensorBase
{
    private:
        TwoWire* _wire;
        uint8_t distBytes[3];
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
        float readDistance() {
            this->distBytes[0] = 0;
            this->distBytes[1] = 0;
            this->distBytes[2] = 0;
            this->doPlex();
            this->_wire->beginTransmission(ULTRASONIC_I2C_ADDR);
            this->_wire->write(1);
            this->_wire->endTransmission();
            delay(120);
            this->_wire->requestFrom(ULTRASONIC_I2C_ADDR, 3);
            int i = 0;
            while (this->_wire->available()) {
                distBytes[i++] = this->_wire->read();
            }
            unsigned long distance;
            distance = (unsigned long)(distBytes[0]) * 65536;
                distance = distance + (unsigned long)(distBytes[1]) * 256;
                distance = (distance + (unsigned long)(distBytes[2])) / 10000;
            if (distance == 0 || distance > 900) {
                return NAN;
            }
            return distance;
        }
        float readAverage() {
            float sum = 0;
            int ctr = 0;
            for (byte i = 0; i < 15 && ctr < 8; i++) {
                float reading = this->readDistance();
                if (isnan(reading)) {
                    continue;
                }
                sum += reading;
                ctr++;
            }
            if (ctr < 4)  {
                return NAN;
            } 
            return sum/ctr;
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
        float readLevelPercent() {
            int cmDist = this->readAverage();
            dbg.printf("ultrasonic sensor cm distance: %d\n", cmDist);
            if (cmDist == NAN || cmDist < 1 || cmDist > 100) {
                return NAN;
            }
            if (cmDist < this->m_fullCm) {
                return 100;
            }
            if (cmDist > this->m_emptyCm) {
                return 0;
            }
            int offs = cmDist - this->m_fullCm;
            int pct = 100 - ((offs* 100) / this->m_range );
            return pct;
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
            this->_wire->beginTransmission(ULTRASONIC_I2C_ADDR);
            this->_wire->write(1);
            this->_wire->endTransmission();
            delay(150);
            this->_wire->requestFrom(0x57, 3);
            this->distBytes[0] = 0;
            this->distBytes[1] = 0;
            this->distBytes[2] = 0;
            int i = 0;
            while (this->_wire->available()) {
                distBytes[i++] = this->_wire->read();
            }
            interrupts();
            unsigned long distance;
            distance = (unsigned long)(distBytes[0]) * 65536;
                distance = distance + (unsigned long)(distBytes[1]) * 256;
                distance = (distance + (unsigned long)(distBytes[2])) / 10000;
            if (distance < ULTRASONIC_MIN_VALID || distance > ULTRASONIC_MAX_VALID) {
                dbg.printf("ultrasonic bad number %lu\n", distance);
                reading.values[0] = NAN;
                reading.isSuccessful = false;
            } else {
                auto cmDist = distance;
                if (cmDist == NAN) {
                    reading.values[0] = NAN;
                    reading.isSuccessful = false;                    
                } else if (cmDist < this->m_fullCm) {
                    reading.values[0] = 100;
                    reading.isSuccessful = true;
                } else if (cmDist > this->m_emptyCm) {
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
            return reading;
        }
        void finishRead(DeferredReading &reading) {
            //has to be done sync because it messes up other sensors on the bus for whatever magical electrical reason
        }
};



#endif