#include "EzoSensor.h"
#include "GrowbotData.h"
#ifndef PHSENSOR_H
#define PHSENSOR_H
class PhSensor : public EzoSensor {
    public:
        PhSensor(I2CMultiplexer* multiplexer, byte multiplexer_bus) : EzoSensor::EzoSensor(multiplexer, multiplexer_bus, "PH", 0x63) {
        }
        PhSensor() : EzoSensor::EzoSensor("PH", 0x63) {
        }
      
        bool read(WaterData &output) {
            float reading = this->readSensor();
            output.ph = reading;
            return (reading != NAN);
        }   
        bool calibrate(byte point, float reference) {
            if (point != EZO_CALIBRATE_MID && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH) {
                return false;
            }
            return this->calibrateSensor(point, reference);
        } 
        // void init() {
        //     this->EzoSensor::init();
        // }
};

#endif