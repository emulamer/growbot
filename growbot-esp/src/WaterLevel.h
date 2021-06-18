#include <Arduino.h>
#include "I2CMultiplexer.h"
#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define ULTRASONIC_I2C_ADDR 0x57
struct WaterLevelCalibration {
    //centimeters of distance when bucket is full
    int fullCm;

    //centimeters of distance when bucket is empty
    int emptyCm;
};
class WaterLevel
{
    private:
        uint8_t distBytes[3];
        int m_emptyCm, m_fullCm;
        int m_range;
        I2CMultiplexer* plexer;
        byte busNum;
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
            Wire.beginTransmission(ULTRASONIC_I2C_ADDR);
            Wire.write(1);
            Wire.endTransmission();
            delay(120);
            Wire.requestFrom(0x57, 3);
            int i = 0;
            while (Wire.available()) {
                distBytes[i++] = Wire.read();
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
        WaterLevel() {
            this->m_fullCm = 1;
            this->m_emptyCm = 100;
            this->m_range = 100;
            this->isMultiplexer = false;
        }
        WaterLevel(WaterLevelCalibration calibration) {
            this->isMultiplexer = false;
            this->setCalibration(calibration);
        }
        WaterLevel(I2CMultiplexer* multiplexer, byte multiplexer_bus) {
            this->m_fullCm = 1;
            this->m_emptyCm = 100;
            this->m_range = 100;
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus; 
        }
        WaterLevel(I2CMultiplexer* multiplexer, byte multiplexer_bus, WaterLevelCalibration calibration) {
            this->setCalibration(calibration);
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus; 
        }
        
        float readLevelPercent() {
            int cmDist = this->readAverage();
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
};



#endif