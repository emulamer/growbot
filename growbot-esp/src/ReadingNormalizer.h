#include <Arduino.h>
#include "DebugUtils.h"

#ifndef READINGNORMALIZER_H
#define READINGNORMALIZER_H


class ReadingNormalizer {
    private:
        const char* name;
        int maxSamples;
        float* samples;
        float maxDeviance;
        int sampleCount = 0;
        int maxBackfill;
        int readFailCount = 0;
        float maxValue;
        float minValue;
        int sampleIndex = 0;
        float lastGoodRead = NAN;
        bool validateReading(float reading) {
            if (isnan(reading) || reading < this->minValue || reading > this->maxValue) {
                dbg.printf("Normalizer: reading on %s is NaN\n", this->name);
                return false;
            }
            return true;
        }
        bool deviates(float reading) {
            if (this->sampleCount > 1) {
                float avg = this->getAvgReading();
                float diff = abs(reading - avg);
                if (diff >= 1 && diff > (this->maxDeviance * avg)) {
                    dbg.printf("Normalizer: reading on %s of %f deviates too much from avg of %f\n", this->name, reading, avg);
                    return true;
                }
            }
            return false;
        }
        float getAvgReading() {
            if (this->sampleCount < 1) {
                return NAN;
            }
            float sum = 0;
            for (int i = 0; i < this->sampleCount; i++) {
                sum += this->samples[i];
            }
            return sum/(float)(this->sampleCount);
        }
    public:
        ReadingNormalizer(const char* readingName, int numSamples, int maxBackfillReadings, float maxDeviancePercent, float minValidValue, float maxValidValue) {
            this->name = readingName;
            this->maxSamples = numSamples;
            this->samples = (float*)malloc(4 * numSamples);
            this->maxDeviance = maxDeviancePercent;
            this->maxBackfill = maxBackfillReadings;
            this->minValue = minValidValue;
            this->maxValue = maxValidValue;
        }

        float normalizeReading(float reading) {
            if (!this->validateReading(reading)) {
                this->readFailCount++;
                //if it's failed more than the number of max backfills, send back a NAN
                if (this->readFailCount > this->maxBackfill) {
                    dbg.printf("Normalizer: Reading on %s of %f was invalid and there are too many failed!\n", this->name, reading);
                    return NAN;
                }
                dbg.printf("Normalizer: Reading on %s of %f was invalid, returning last good value: %f\n", this->name, reading, this->lastGoodRead);
                return this->lastGoodRead;
            } 
            
            if (sampleCount < 1) {
                sampleCount++;
                sampleIndex = 0;
            } else {
                sampleIndex++;
                if (this->sampleCount < (sampleIndex+1)) {
                    this->sampleCount++;
                }
                if (sampleIndex >= this->maxSamples) {
                    sampleIndex = 0;
                }
            }                
            this->samples[sampleIndex] = reading;

            if (this->deviates(reading)) {
                this->readFailCount++;
                if (this->readFailCount > this->maxBackfill) {
                    dbg.printf("Normalizer: Reading on %s of %f was invalid and there are too many failed!\n", this->name, reading);
                    return NAN;
                }
                dbg.printf("Normalizer: Reading on %s of %f deviates too far!  Returning last good value %f\n", this->name, reading, this->lastGoodRead);
                return this->lastGoodRead;
            
            } else {
                this->readFailCount = 0;
                dbg.printf("good reading on %s, setting last good read to %f\n", this->name, reading);
                this->lastGoodRead = reading;                
            }     
            
            return reading;       
        }        
};


#endif