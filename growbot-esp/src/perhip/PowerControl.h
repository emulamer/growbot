#include <Arduino.h>
#include <Wire.h>
#include "I2CMultiplexer.h"
#include "../GrowbotData.h"
#include "../DebugUtils.h"
#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#define PORT_ADDR_OFFSET 0x80;
#define TIMER_TICK_INTERVAL_MS 1000;

//thanks to anonymous amazon customer for this map
const byte levelMap[] = {
100, 66, 65, 61, 59, 57, 55, 53, 51,
50, 48, 47, 46, 44, 43, 42, 41,
39, 38, 37, 35, 34, 33, 32, 30,
28, 27, 25, 23, 21, 18, 14, 0};


class PowerControl {
    private:
        I2CMultiplexer* plexer;
        int busNum;
        bool isMultiplexer;
        TwoWire* _wire;
        bool toggledOn[4];
        byte m_address;
        int m_targetIdx[4];
        int m_channelLevel[4];
        PowerControlCalibration m_calibrations[4];
        unsigned long spinUpEndStamp[4];
        bool timerRunning;
        bool doingSomething = false;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        }

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
       //     dbg.printf("DIRECT: %d => %d\r\n", portNum, idx);
            byte portAddr = portNum + PORT_ADDR_OFFSET;
            byte val = levelMap[idx];
            this->doPlex();
            this->_wire->beginTransmission(this->m_address);
            this->_wire->write(portAddr);
            this->_wire->write(val);
            this->_wire->endTransmission();
        }
        void onTimerTick() {
            //dbg.println("power timer tick");
            if (doingSomething) {
          //      dbg.print("\t\ttmr: in use, skip\r\n");
                return;
            }
            bool stopTimer = true;
            for (byte i = 0; i < 4; i++) {
                if (this->spinUpEndStamp[i] > 0) {
                    // dbg.print(i);
                    // dbg.print("endstamp is ");
                    // dbg.print(this->spinUpEndStamp[i]);
                    // dbg.print(" current millis is ");
                    // dbg.println(millis());
                    if (this->spinUpEndStamp[i] < millis()) {
                        dbg.printf("\t\tpowertmr: setting %d => %d\r\n", i, this->m_targetIdx[i]);
                        this->setChannelIdxDirect(i, this->m_targetIdx[i]);
                        this->spinUpEndStamp[i] = 0;
                    } else {
                        //dbg.printf("\t\ttmr: port %d still in future\r\n", i);
                        stopTimer = false;
                    }
                }
            }
            if (stopTimer) {
            //    dbg.println("\t\ttmr: timer stopping");
                this->timerRunning = false;
            }            
        }
        void setChannelLevelInternal(byte portNum, int level) {
            doingSomething = true;
        //    dbg.printf("\tint: setting channel %d => %d pct\r\n", portNum, level);
            int idx = this->getIdx(portNum, level);
        //     if (idx == this->m_targetIdx[portNum]) {
        //    //     dbg.print("\tint: already set\r\n");
        //         //don't do anything if it's set to the same index
        //         doingSomething = false;
        //         return;
        //     }
            bool coldStart = this->m_targetIdx[portNum] == 0 && this->spinUpEndStamp[portNum] == 0;
            this->m_targetIdx[portNum] = idx;
            if (idx == 0 || idx == 32 || this->m_calibrations[portNum].spinUpSec < 1) {
                //just set it and cancel any previous spin up timer
             //   dbg.print("\tint: idx is max or min or it has no spin up sec\r\n");
                this->spinUpEndStamp[portNum] = 0;
                this->setChannelIdxDirect(portNum, idx);
            } else if (coldStart) {
            //    dbg.printf("\tint: doing timer set for port %d to idx %d after spinupsec %d\r\n", portNum, idx, this->m_calibrations[portNum].spinUpSec);
                this->setChannelIdxDirect(portNum, 32);
                this->spinUpEndStamp[portNum] = millis() + (unsigned long)(this->m_calibrations[portNum].spinUpSec * 1000);
             //   dbg.printf("\tint: current stamp %d, target stamp %d\r\n", millis(), this->spinUpEndStamp[portNum]);
                if (!this->timerRunning) {
             //       dbg.printf("\tint: starting timer\r\n");
                    this->timerRunning = true;
                } else {
             //       dbg.printf("\tint: timer already running\r\n");
                }
            } else if (this->spinUpEndStamp[portNum] == 0) {
            //    dbg.print("\tint: not a cold start, direct set\r\n");
                this->setChannelIdxDirect(portNum, idx);
            }
            doingSomething = false;
        }
    public:
        PowerControl(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus, byte address) {
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
            this->isMultiplexer = true;
            this->_wire = wire;
            this->m_address = address;
            this->timerRunning = false;
        }
        PowerControl(TwoWire* wire, byte address) {
            this->isMultiplexer = false;
            this->_wire = wire;
            this->m_address = address;
            this->timerRunning = false;
                     
        }
        void init() {
            for (byte i = 0; i < 4; i++) {
                this->m_calibrations[i].maxOffset = 31;
                this->m_calibrations[i].minOffset = 1;
                this->m_calibrations[i].spinUpSec = 0;
                this->m_targetIdx[i] = 0;            
                this->m_channelLevel[i] = 0;
                this->toggledOn[i] = true;
                this->setChannelIdxDirect(i, 32);
                if (this->m_calibrations[i].spinUpSec > 0) {
                    this->spinUpEndStamp[i] = millis() + (unsigned long)(this->m_calibrations[i].spinUpSec * 1000);
                    this->timerRunning = true;
                }
            }   
        }
        void handle() {
            if (!this->timerRunning) {
                return;
            }
            this->onTimerTick();
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
                dbg.printf("power set port %d to level %d\r\n", portNum, level);
                this->setChannelLevelInternal(portNum, level);
            } else {
                dbg.printf("power set port %d to level %d, but power toggle is off\r\n", portNum, level);
                this->setChannelLevelInternal(portNum, 0);
            }
        }        
};


#endif