#include <Arduino.h>
#include "GrowbotData.h"
#include <functional>
#include "Sensorama.h"

#ifndef THERMOSTAT_H
#define THERMOSTAT_H
class Thermostat {
    protected:
        std::function<void(bool, float)> updateCallback;
    public:
        virtual void handle() = 0;
};

class FixedThermostat : public Thermostat {
    private:
        bool currentState = false;
        float* sourceValue;
        FixedThermostatSetting* settings;
    public:
        void init(FixedThermostatSetting* settings, float* sourceValue, std::function<void(bool, float)> updateCallback) {
            this->settings = settings;
            this->updateCallback = updateCallback;
            this->sourceValue = sourceValue;
        }
        virtual void handle() {
            if (settings->mode == ThermostatMode::Off) {
                dbg.printf("Theromostat set off, currentState set to false\n");
                currentState = false;
            } else if (settings->mode == ThermostatMode::On) {
                dbg.printf("Theromostat set on, currentState set to false\n");
                currentState = true;
            } else {
                if (*sourceValue < settings->minValue && currentState == true) {
                    dbg.printf("Theromostat value %f dropped below min %f, currentState set to false\n", *sourceValue, settings->minValue);
                    currentState = false;
                } else if (*sourceValue > settings->maxValue && currentState == false) {
                    dbg.printf("Theromostat value %f went above max %f, currentState set to true\n", *sourceValue, settings->maxValue);
                    currentState = true;
                }
            }
            updateCallback(currentState, currentState?100:0);
        }


};


#endif