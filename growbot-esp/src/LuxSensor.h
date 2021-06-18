#include <Arduino.h>
#include <SparkFun_SHTC3.h>
#include "I2CMultiplexer.h"
#include "GrowbotData.h"
#include <Max44009.h>
#ifndef LUXSENSOR_H
#define LUXSENSOR_H

uint8_t CDR = 0;
uint8_t TIM = 0;

class LuxSensor {
    public:
        virtual void init();
        virtual float readLux();
};

class Max44009Sensor : public LuxSensor {
    private:
        Max44009 myLux = Max44009(false);
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
        bool isMultiplexer;

    public: 
        Max44009Sensor(I2CMultiplexer* multiplexer, byte multiplexer_bus) {
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        Max44009Sensor() {
            this->isMultiplexer = false;
        }
        void init() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            this->myLux.setManualMode(CDR, TIM);
            int err = this->myLux.getError();
            if (err != 0)
            {
                Serial.print("failed to init lux sensor: ");
                Serial.println(err);
                this->isOk = false;
            } else {
                this->isOk = true;
            }
        }
        float readLux() {
            if (!this->isOk) {
                this->init();
            }
            if (!this->isOk) {
                return NAN;
            }
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            float lux = myLux.getLux();
            int err = myLux.getError();
            if (err != 0)
            {
                Serial.print("lux sensor failed to read: ");
                Serial.println(err);
                return NAN;
            } else {
                return lux;
            }
        }
};


#endif