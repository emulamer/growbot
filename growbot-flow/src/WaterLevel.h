#include <Arduino.h>
#include <ESP32AnalogRead.h>

#pragma once

#define WATERLEVEL_READ_COUNT 10
#define WATERLEVEL_ROLLING_AVG_COUNT 10

#define WATERLEVEL_REF_RESIST_BASE 1750
#define WATERLEVEL_VAL_RESIST_EMPTY 1790
#define WATERLEVEL_VAL_RESIST_FULL 450

class WaterLevel {
    private:
        float samples[WATERLEVEL_ROLLING_AVG_COUNT];
        bool currentAverageValid = false;
        float currentAverage = 0;
        unsigned long lastSampleStamp = 0;
        int lastSampleIndex = -1;
        bool samplesFull = false;        
        ESP32AnalogRead adcRef;
        ESP32AnalogRead adcVal;
        int sampleIntervalMS;
        float readWaterLevel() {
            float refRead = 0;
            float valRead = 0;
            for (int i = 0; ; i++) {
                float ref = adcRef.readVoltage(); 
                float val = adcVal.readVoltage();
                refRead += ref;
                valRead += val;
                if (i == WATERLEVEL_READ_COUNT-1) {
                    break;
                }
            }
            refRead = refRead / (float)WATERLEVEL_READ_COUNT;
            valRead = valRead / (float)WATERLEVEL_READ_COUNT;
            float refOhms =  ((1500 * 3.3) / refRead) - 1500;
            float valOhms = ((1500 * 3.3) / valRead) - 1500;

            float adjOhms = WATERLEVEL_REF_RESIST_BASE/refOhms;
            float adjustedValOhms = valOhms * adjOhms;
            float percent = (1.0 - (((adjustedValOhms - WATERLEVEL_VAL_RESIST_FULL) / (float)(WATERLEVEL_VAL_RESIST_EMPTY - WATERLEVEL_VAL_RESIST_FULL)))) * 100.0;
            //dbg.printf("Ref: %f, Val: %f, refadj: %f, adjval: %f, percent: %f\n", refOhms, valOhms, adjOhms, adjustedValOhms, percent);
            return percent;
        }

    public:
        WaterLevel(byte refPin, byte valPin, int sampleIntervalMS) {
            adcRef.attach(refPin);
            adcVal.attach(valPin);
            this->sampleIntervalMS = sampleIntervalMS;
        }

        void update() {
            if (millis() - lastSampleStamp > sampleIntervalMS) {
                lastSampleIndex++;
                if (lastSampleIndex >= WATERLEVEL_ROLLING_AVG_COUNT) {
                lastSampleIndex = 0;
                samplesFull = true;
                }
                samples[lastSampleIndex] = readWaterLevel();
                lastSampleStamp = millis();
                currentAverageValid = false;
            }   
        }

        float getImmediateLevel() {
            return readWaterLevel();
        }

        float getWaterLevel() {
            if (currentAverageValid) {
                return currentAverage;
            }
            int idx = lastSampleIndex;
            float sum = 0;
            int ct = 0;
            for (int i = 0; i < WATERLEVEL_ROLLING_AVG_COUNT; i++) {
                sum += samples[idx];
                ct++;
                idx--;
                if (idx < 0) {
                    if (!samplesFull) {
                        break;
                    } else {
                        idx = WATERLEVEL_ROLLING_AVG_COUNT - 1;
                    }
                }
            }
            if (ct < 1) {
                currentAverage = 0;  
            } else {
                currentAverage = sum/ct;
            }
            if (currentAverage < 0) {
                currentAverage = 0;
            } else if (currentAverage > 100) {
                currentAverage = 100;
            }
            currentAverageValid = true;
            return currentAverage;
        }

};