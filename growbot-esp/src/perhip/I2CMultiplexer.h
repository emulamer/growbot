#include <Arduino.h>
#include <Wire.h>
#include "../DebugUtils.h"

#ifndef I2CMULTIPLEXER_H
#define I2CMULTIPLEXER_H
 
class I2CMultiplexer {
    private:
        byte address1;
        byte address2;
        TwoWire* _wire;
        int currentBus = -1;
        void setAddressBus(byte address, byte bus) {
            this->_wire->beginTransmission(address);
            this->_wire->write(bus);
            this->_wire->endTransmission();
            delay(10);
        }
    public:
        I2CMultiplexer(TwoWire* wire, byte multiplexer1_address, byte multiplexer2_address) {
            this->_wire = wire;
            this->address1 = multiplexer1_address;
            this->address2 = multiplexer2_address;
            this->currentBus = -1;
        }


        void setBus(int bus) {
            if (this->currentBus == bus) {
                return;
            }
            int val = 1<<bus;
            this->setAddressBus(this->address1, (byte)(val & 0xFF));
            this->setAddressBus(this->address2, (byte)((val>>8) & 0xFF));

            this->currentBus = bus;
        }
};
 


#endif