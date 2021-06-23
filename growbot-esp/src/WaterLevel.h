#include <Arduino.h>
#include "I2CMultiplexer.h"
#include "DebugUtils.h"
#include "DeferredSensorReader.h"
#include <Ticker.h>

#ifndef WATERLEVEL_H
#define WATERLEVEL_H

#define ULTRASONIC_I2C_ADDR 0x57

#define ULTRASONIC_NUM_SAMPLES 5
#define ULTRASONIC_MIN_VALID 1
#define ULTRASONIC_MAX_VALID 500
#define ULTRASONIC_READ_DELAY_MS 120

struct WaterLevelCalibration {
    //centimeters of distance when bucket is full
    int fullCm;

    //centimeters of distance when bucket is empty
    int emptyCm;
};
class WaterLevel : public DeferredSensorReader
{
    private:
        uint8_t distBytes[3];
        int m_emptyCm, m_fullCm;
        int m_range;
        I2CMultiplexer* plexer;
        byte busNum;
        bool isMultiplexer;
       // Ticker samplerTimer;
        int samplerCounter = -1;
        int sumCounter = 0;
        float samplerSum = 0;
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
            dbg.print("cm dist");
            dbg.println(cmDist);
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
        void onReadTick() {
            dbg.println("ultrasonic sample timer tick");
            this->doPlex();
            Wire.requestFrom(0x57, 3);
            this->distBytes[0] = 0;
            this->distBytes[1] = 0;
            this->distBytes[2] = 0;
            int i = 0;
            while (Wire.available()) {
                distBytes[i++] = Wire.read();
            }
            unsigned long distance;
            distance = (unsigned long)(distBytes[0]) * 65536;
                distance = distance + (unsigned long)(distBytes[1]) * 256;
                distance = (distance + (unsigned long)(distBytes[2])) / 10000;
            if (!(distance < ULTRASONIC_MIN_VALID || distance > ULTRASONIC_MAX_VALID)) {
                this->samplerSum += distance;
                this->sumCounter++;
            }
            dbg.printf("read cm %lu\n", distance);
            this->samplerCounter++;
            if (samplerCounter > ULTRASONIC_NUM_SAMPLES) {
                this->samplerCounter = -1;
                //this->samplerTimer.detach();
                dbg.println("ultrasonic reached sample count, stopping timer");
            } else {
                this->doPlex();
                Wire.beginTransmission(ULTRASONIC_I2C_ADDR);
                Wire.write(1);
                Wire.endTransmission();
            }
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.sourceSensor = this;
            reading.isComplete = false;
            reading.isSuccessful = false;
            reading.readingCount = 1;
            reading.values[0] = NAN;
            //if samplerCounter is already set, there's another reading in progress
            // if (this->samplerCounter >= 0) {
            //     dbg.println("ultrasonic sampler is already running!");
            //     reading.isComplete = true;
            //     reading.deferUntil = 0;
            // }
            // this->samplerCounter = 0;
            // this->samplerSum = 0;
            // this->sumCounter = 0;
            reading.deferUntil = 0;//millis() + (ULTRASONIC_NUM_SAMPLES * ULTRASONIC_READ_DELAY_MS) + 100;
            //dbg.println("ultrasonic starting read timer");
            this->doPlex();
            Wire.beginTransmission(ULTRASONIC_I2C_ADDR);
            Wire.write(1);
            Wire.endTransmission();
            delay(150);
            Wire.requestFrom(0x57, 3);
            this->distBytes[0] = 0;
            this->distBytes[1] = 0;
            this->distBytes[2] = 0;
            int i = 0;
            while (Wire.available()) {
                distBytes[i++] = Wire.read();
            }
            interrupts();
            unsigned long distance;
            distance = (unsigned long)(distBytes[0]) * 65536;
                distance = distance + (unsigned long)(distBytes[1]) * 256;
                distance = (distance + (unsigned long)(distBytes[2])) / 10000;
            if (distance < ULTRASONIC_MIN_VALID || distance > ULTRASONIC_MAX_VALID) {
                dbg.printf("ultrasonic bad number %lu", distance);
                reading.values[0] = NAN;
                reading.isSuccessful = false;
            } else {
                auto cmDist = distance;
                // if (this->sumCounter < 1) {
                //     reading.isSuccessful = false;
                //     reading.isComplete = true;
                //     reading.values[0] = NAN;
                //     dbg.println("ultrasonic sampler got no samples!");
                //} else {
                //float cmDist = this->samplerSum / this->sumCounter;
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
           // delay(3000);
           // this->samplerTimer.attach_ms(ULTRASONIC_READ_DELAY_MS,  +[](WaterLevel* instance) { instance->onReadTick(); }, this);            
            return reading;
        }
        void finishRead(DeferredReading &reading) {
            //dbg.println("ultrasonic sampler finish read called");
            // this->doPlex();
            // Wire.requestFrom(0x57, 3);
            // this->distBytes[0] = 0;
            // this->distBytes[1] = 0;
            // this->distBytes[2] = 0;
            // int i = 0;
            // while (Wire.available()) {
            //     distBytes[i++] = Wire.read();
            // }
            // interrupts();
            // unsigned long distance;
            // distance = (unsigned long)(distBytes[0]) * 65536;
            //     distance = distance + (unsigned long)(distBytes[1]) * 256;
            //     distance = (distance + (unsigned long)(distBytes[2])) / 10000;
            // if (distance < ULTRASONIC_MIN_VALID || distance > ULTRASONIC_MAX_VALID) {
            //     reading.values[0] = NAN;
            //     reading.isSuccessful = false;
            //     return;
            // } else {
            //     auto cmDist = distance;
            //     // if (this->sumCounter < 1) {
            //     //     reading.isSuccessful = false;
            //     //     reading.isComplete = true;
            //     //     reading.values[0] = NAN;
            //     //     dbg.println("ultrasonic sampler got no samples!");
            //     //} else {
            //     //float cmDist = this->samplerSum / this->sumCounter;
            //     if (cmDist == NAN) {
            //         reading.values[0] = NAN;
            //         reading.isSuccessful = false;                    
            //     } else if (cmDist < this->m_fullCm) {
            //         reading.values[0] = 100;
            //         reading.isSuccessful = true;
            //     } else if (cmDist > this->m_emptyCm) {
            //         reading.values[0] = 0;
            //         reading.isSuccessful = true;
            //     } else {
            //         float offs = cmDist - this->m_fullCm;
            //         float pct = 100 - ((offs* 100) / (float)(this->m_range));
            //         reading.values[0] = pct;
            //         reading.isSuccessful = true;
            //     }
            // }
            // reading.isComplete = true;
            //}           
        }
};



#endif