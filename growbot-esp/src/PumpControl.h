#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include "GrowbotData.h"
#include "perhip/SwitcherooWiFi.h"
#include "DebugUtils.h"

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#define MIN_WATER_LEVEL_PERCENT 6
class PumpControl {
    private:
        GrowbotData* growbotData;
        bool* intendedPumpState;
        byte portNum;
        SwitcherooWiFi* switcher;
    public:
        PumpControl(SwitcherooWiFi* switcher, byte portNum, GrowbotData* growbotData, bool* intendedPumpState) {
            this->switcher = switcher;
            this->portNum = portNum;
            this->growbotData = growbotData;
            this->intendedPumpState = intendedPumpState;
        }

        void handle() {
            bool emShutoff = false;
            if (this->growbotData->controlBucket.waterLevelPercent < MIN_WATER_LEVEL_PERCENT) {
                emShutoff = true;
            }
           //TODO: integration with water leak detectors somehow

            if (*this->intendedPumpState && !emShutoff) {
                this->growbotData->pumpOn = true;   
            } else {
                this->growbotData->pumpOn = false;
            }
            this->growbotData->pumpEmergencyShutoff = emShutoff;
            if (this->switcher->getPowerToggle(this->portNum) != this->growbotData->pumpOn) {
                dbg.printf("PumpControl: changing port %d to state %s\n", this->portNum, this->growbotData->pumpOn?"ON":"OFF");
            }
            this->switcher->setPowerToggle(this->portNum, this->growbotData->pumpOn);
        }
};



#endif