#include "GrowbotData.h"
#include "Config.h"

#ifndef NVSTORE_H
#define NVSTORE_H

    class NVStore {
        public:
            virtual void readConfig(GrowbotConfig* config) = 0;
            virtual void writeConfig(GrowbotConfig &config) = 0;
            virtual void readDefaultConfig(GrowbotConfig* config) {
                config->exhaustFanPercent = 100;
                config->intakeFanPercent = 100;
                config->exhaustFanCalibration.maxOffset = 30;
                config->exhaustFanCalibration.minOffset = 1;
                config->intakeFanCalibration.maxOffset = 30;
                config->intakeFanCalibration.minOffset = 1;
                config->waterChillerThermostat.mode = ThermostatMode::Thermostat;
                config->waterChillerThermostat.minValue = 0;
                config->waterChillerThermostat.maxValue = 100;
                config->samplingIntervalMS = 10000;
                config->exhaustFanOn = true;
                config->intakeFanOn = true;
                config->roomFanSchedule.status = 2;
                config->roomFanSchedule.turnOffGmtHourMin = 0;
                config->roomFanSchedule.turnOnGmtHourMin = 0;
                config->lightSchedule.status = 2;
                config->lightSchedule.turnOffGmtHourMin = 0;
                config->lightSchedule.turnOnGmtHourMin = 0;                
            }
    };


#endif