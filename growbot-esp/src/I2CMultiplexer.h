#include <Arduino.h>
#include <Wire.h>

#ifndef I2CMULTIPLEXER_H
#define I2CMULTIPLEXER_H
 
class I2CMultiplexer {
    private:
        byte address;

    public:
        I2CMultiplexer(byte multiplexer_address) {
            this->address = multiplexer_address;
        }

        void setBus(byte bus) {
            Wire.beginTransmission(this->address);
            Wire.write(1<<bus);
            Wire.endTransmission();
        }
};
 


#endif