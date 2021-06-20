#include <Arduino.h>
#include <Wire.h>
#include <Ticker.h>
#include "GrowbotData.h"

#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#define PORT_ADDR_OFFSET 0x80;

//thanks to anonymous amazon customer for this map
const byte levelMap[] = {
100, 66, 65, 61, 59, 57, 55, 53, 51,
50, 48, 47, 46, 44, 43, 42, 41,
39, 38, 37, 35, 34, 33, 32, 30,
28, 27, 25, 23, 21, 18, 14, 0};


class PowerControl {
    private:
        bool toggledOn[4];
        byte m_address;
        int m_targetIdx[4];
        int m_channelLevel[4];
        PowerControlCalibration m_calibrations[4];
        Ticker spinUpTimer;
        unsigned long spinUpEndStamp[4];
        bool timerRunning;

        int getIdx(byte portNum, int level) {
            int idx = 0;
            if (level <= 0) {
                idx = 0;
            } else if (level >= 100) {
                idx = 32;
            } else { 
                idx = (level * this->m_calibrations[portNum].getRange())/ 98;
                idx += this->m_calibrations[portNum].minOffset;
            }
            return idx;
        }
        void setChannelIdxDirect(byte portNum, int idx) {
            byte portAddr = portNum + PORT_ADDR_OFFSET;
            byte val = levelMap[idx];
            Wire.beginTransmission(this->m_address);
            Wire.write(portAddr);
            Wire.write(val);
            Wire.endTransmission();
        }
        void onTimerTick() {
            Serial.println("power timer tick");
            bool stopTimer = true;
            for (byte i = 0; i < 4; i++) {
                if (this->spinUpEndStamp[i] > 0) {
                    if (this->spinUpEndStamp[i] < millis()) {
                        Serial.println("doing a thing for power timer");
                        this->setChannelIdxDirect(i, this->m_targetIdx[i]);
                        this->spinUpEndStamp[i] = 0;
                    } else {
                        stopTimer = false;
                    }
                }
            }
            if (stopTimer) {
                Serial.println("power timer stopping");
                this->spinUpTimer.detach();
                this->timerRunning = false;
            }            
        }
        void setChannelLevelInternal(byte portNum, int level) {
            int idx = this->getIdx(portNum, level);
            if (idx == this->m_targetIdx[portNum]) {
                //don't do anything if it's set to the same index
                return;
            }
            this->m_targetIdx[portNum] = idx;
            if (idx == 0 || idx == 32 || this->m_calibrations[portNum].spinUpSec < 1) {
                //just set it and cancel any previous spin up timer
                this->spinUpEndStamp[portNum] = 0;
                this->setChannelIdxDirect(portNum, idx);
            } else if (this->spinUpEndStamp[portNum] == 0) {
                this->setChannelIdxDirect(portNum, 32);
                this->spinUpEndStamp[portNum] = millis() + (unsigned long)(this->m_calibrations[portNum].spinUpSec * 1000);
                if (!this->timerRunning) {
                    this->timerRunning = true;
                    this->spinUpTimer.attach(1,  +[](PowerControl* instance) { instance->onTimerTick(); }, this);
                }
            }            
        }
    public:
        PowerControl(byte address) {
            this->m_address = address;
            this->timerRunning = false;
            for (byte i = 0; i < 4; i++) {
                this->m_calibrations[i].maxOffset = 31;
                this->m_calibrations[i].minOffset = 1;
                this->m_calibrations[i].spinUpSec = 0;
                this->m_targetIdx[i] = 32;            
                this->m_channelLevel[i] = 100;
                this->toggledOn[i] = true;
                this->setChannelIdxDirect(i, 32);
                if (this->m_calibrations[i].spinUpSec > 0) {
                    this->spinUpEndStamp[i] = millis() + (unsigned long)(this->m_calibrations[i].spinUpSec * 1000);
                    if (!this->timerRunning) {
                        this->timerRunning = true;
                        this->spinUpTimer.attach(1,  +[](PowerControl* instance) { instance->onTimerTick(); }, this);
                    }
                }
            }            
        }

        void setPowerCalibration(byte port, PowerControlCalibration &calibration, bool updateLevel) {
            if (port > 3) {
                return;
            }
            this->m_calibrations[port] = calibration;
            if (this->m_calibrations[port].maxOffset > 31) {
                this->m_calibrations[port].maxOffset = 31;
            }
            if (this->m_calibrations[port].maxOffset < 2) {
                this->m_calibrations[port].maxOffset = 2;
            }
            if (this->m_calibrations[port].minOffset >= this->m_calibrations[port].maxOffset) {
                this->m_calibrations[port].minOffset = this->m_calibrations[port].maxOffset - 1;
            }
            if (updateLevel) {
                if (this->toggledOn[port]) {
                    this->setChannelLevelInternal(port, this->m_channelLevel[port]);
                } else {
                    this->setChannelLevelInternal(port, 0);
                }    
            }
        }
 
        void setPowerToggle(byte portNum, bool on) {
            if (this->toggledOn[portNum]) {
                this->setChannelLevelInternal(portNum, this->m_channelLevel[portNum]);
            } else {
                this->setChannelLevelInternal(portNum, 0);
            }
        }

        void setChannelLevel(byte portNum, int level) {
            if (portNum > 3) {
                return;
            }
            if (level > 100) {
                level = 100;
            } else if (level < 0) {
                level = 0;
            }
            this->m_channelLevel[portNum] = level;
            if (this->toggledOn[portNum]) {
                this->setChannelLevelInternal(portNum, level);
            } else {
                this->setChannelLevelInternal(portNum, 0);
            }
        }        
};


#endif