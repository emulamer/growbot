#include <Arduino.h>
#include <Wire.h>

#ifndef I2CMULTIPLEXER_H
#define I2CMULTIPLEXER_H
 
class I2CMultiplexer {
    private:
        byte address;
        TwoWire* _wire;
        byte currentBus;
    public:
        I2CMultiplexer(TwoWire* wire, byte multiplexer_address) {
            this->_wire = wire;
            this->address = multiplexer_address;
            this->currentBus = -1;
        }

        void setBus(byte bus) {
            // if (this->currentBus == bus) {
            //     return;
            // }
            this->_wire->beginTransmission(this->address);
            this->_wire->write(1<<bus);
            this->_wire->endTransmission();
            this->currentBus = bus;
        }
};
 


#endif