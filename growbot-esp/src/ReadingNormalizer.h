#include <Arduino.h>
#include "DebugUtils.h"

#ifndef READINGNORMALIZER_H
#define READINGNORMALIZER_H


class ReadingNormalizer {
    private:
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
                dbg.printf("Normalizer: reading is NaN\n");
                return false;
            }
            if (this->sampleCount > 0) {
                float avg = this->getAvgReading();
                if (abs(reading - avg) > (this->maxDeviance * avg)) {
                    dbg.printf("Normalizer: reading of %f deviates too much from avg of %f\n", reading, avg);
                    return false;
                }
            }
            return true;
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
        ReadingNormalizer(int numSamples, int maxBackfillReadings, float maxDeviancePercent, float minValidValue, float maxValidValue) {
            this->maxSamples = numSamples;
            this->samples = (float*)malloc(4 * numSamples);
            this->maxDeviance = maxDeviancePercent;
            this->maxBackfill = maxBackfillReadings;
            this->minValue = minValidValue;
            this->maxValue = maxValidValue;
        }

        float normalizeReading(float reading) {
            if (!this->validateReading(reading)) {
                readFailCount++;
                //if it's failed more than the number of max backfills, send back a NAN
                if (readFailCount > this->maxBackfill) {
                    dbg.println("Reading was invalid and there are too many failed!");
                    return NAN;
                }
                dbg.println("Reading was invalid, returning last good value.");
                return lastGoodRead;
            } else {
                this->lastGoodRead = reading;
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
                return reading;
            }            
        }        
};


#endif