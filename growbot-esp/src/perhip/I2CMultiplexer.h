#include <Arduino.h>
#include <Wire.h>
#include "../DebugUtils.h"

#ifndef I2CMULTIPLEXER_H
#define I2CMULTIPLEXER_H
 
class I2CMultiplexer {
    private:
        byte address1;
        byte address2;
        TwoWire* _wire = NULL;
        TwoWire* _wire2 = NULL;
        bool isMultiPort = false;
        int currentBus = -1;
        void setAddressBus(TwoWire* wire, byte address, byte bus) {
            //dbg.printf("Sending address %0x bus %d on wire %d\n", address, bus, (wire == _wire)?0:1);
            wire->beginTransmission(address);
            wire->write(bus);
            wire->endTransmission();
            delay(10);
            wire->flush();
        }
    public:
        I2CMultiplexer(TwoWire* wire, byte multiplexer1_address, byte multiplexer2_address) {
            this->_wire = wire;
            this->isMultiPort = false;
            this->address1 = multiplexer1_address;
            this->address2 = multiplexer2_address;
            this->currentBus = -1;
        }
        I2CMultiplexer(TwoWire* wire, TwoWire* wire2, byte multiplexer_address) {
            this->_wire = wire;
            this->_wire2 = wire2;
            this->isMultiPort = true;
            this->address1 = multiplexer_address;
            this->currentBus = -1;
        }

        void flushAll() {
            if (_wire != NULL) {
                _wire->flush();
            }
            if (_wire2 != NULL) {
                _wire2->flush();
            }
        }
        void setBus(int bus) {
            flushAll();
            if (this->currentBus == bus) {
                return;
            }
            int val = 0;
            if (bus >= 0) {
                val = 1<<bus;
            }
            if (isMultiPort) {
                this->setAddressBus(this->_wire, this->address1, (byte)(val & 0xFF));
                this->setAddressBus(this->_wire2, this->address1, (byte)((val>>8) & 0xFF));
            } else {
                this->setAddressBus(this->_wire, this->address1, (byte)(val & 0xFF));
                this->setAddressBus(this->_wire, this->address2, (byte)((val>>8) & 0xFF));
            }
            flushAll();

            this->currentBus = bus;
        }
};
 


#endif