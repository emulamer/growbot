#include <Arduino.h>
#include <Wire.h>
#include "I2CMultiplexer.h"

#ifndef SWITCHEROO_H
#define SWITCHEROO_H

#define SWITCHEROO_I2C_ADDR 0x28

class Switcheroo {
    private: 
        I2CMultiplexer* plexer;
        int busNum;
        bool isMultiplexer;
        byte powerFlags = 0;
        TwoWire* _wire;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        }
        bool getPowerFlags() {
            this->doPlex();
            int readnum = this->_wire->requestFrom(SWITCHEROO_I2C_ADDR, 1);
            if (readnum != 1) {
                dbg.printf("Failed to request state from switcheroo, wanted 1 got %d\n", readnum);
                return false;
            }
            byte flags = 0;
            if (!this->_wire->available()) {
                dbg.printf("Failed to request state from switcheroo, no data was available!\n");
                return false;
            }
            dbg.printf("Switcheroo: read power flags: %d\n", flags);
            this->powerFlags = flags;
            return true;
        }
    public:
        Switcheroo(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus) {
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
            this->isMultiplexer = true;
            this->_wire = wire;
        }
        Switcheroo(TwoWire* wire) {
            this->isMultiplexer = false;
            this->_wire = wire;
        }
        void init() {
            getPowerFlags();
        }
        bool getPowerToggle(byte portNum) {
            byte flag = 1<<portNum;
            return (flag & this->powerFlags) != 0;
        }
        bool setPowerToggle(byte portNum, bool on) {
            byte flag = 1<<portNum;
            if (on) {
                this->powerFlags |= flag;
            } else {
                this->powerFlags &= ~flag;
            }
            dbg.printf("Switcheroo: setting power flags: %d\n", this->powerFlags);
            this->doPlex();
            this->_wire->beginTransmission(SWITCHEROO_I2C_ADDR);
            this->_wire->write(this->powerFlags);
            byte result = this->_wire->endTransmission();
            if (result != 0) {
                dbg.printf("Switcheroo failed to set port %d to %d, endTransmission result was %d\n", portNum, on, result);
                return false;
            }
            return true;
        }
};

#endif