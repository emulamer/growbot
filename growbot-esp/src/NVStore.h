#include "GrowbotData.h"
#include "Config.h"

#ifndef NVSTORE_H
#define NVSTORE_H

    class NVStore {
        public:
            virtual void readConfig(GrowbotConfig* config) = 0;
            virtual void writeConfig(GrowbotConfig* config) = 0;
            virtual void readDefaultConfig(GrowbotConfig* config) {
                config->exhaustFanPercent = 100;
                config->intakeFanPercent = 100;
                config->pumpPercent = 100;
                config->controlWaterLevelCalibration.emptyCm = 14;
                config->controlWaterLevelCalibration.fullCm = 4;
                for (byte i = 0; i < NUM_BUCKETS; i++) {
                    config->bucketWaterLevelCalibration[i].emptyCm = 14;
                    config->bucketWaterLevelCalibration[i].fullCm = 4;
                }
                config->exhaustFanCalibration.maxOffset = 30;
                config->exhaustFanCalibration.minOffset = 1;
                config->intakeFanCalibration.maxOffset = 30;
                config->intakeFanCalibration.minOffset = 1;
                config->pumpCalibration.maxOffset = 30;
                config->pumpCalibration.minOffset = 1;
                config->samplingIntervalMS = 10000;
                config->pumpOn = true;
                config->exhaustFanOn = true;
                config->intakeFanOn = true;
                config->overheadLightsOn = true;
                config->pumpOn = true;
                config->sideLightsOn = true;
            }
    };


#endif