#include <Arduino.h>
#include <SparkFun_SHTC3.h>
#include "I2CMultiplexer.h"
#include "../GrowbotData.h"
#include <Max44009.h>
#include "../DebugUtils.h"
#include "../SensorBase.h"
#define LUX_SENSOR_ADDR 0x4a
#ifndef LUXSENSOR_H
#define LUXSENSOR_H

uint8_t CDR = 0;
uint8_t TIM = 0;

class LuxSensor : public SensorBase {
    public:
        virtual void init();
        virtual float readLux();
};

class Max44009Sensor : public LuxSensor {
    private:
        TwoWire* _wire;
        Max44009 myLux = Max44009(LUX_SENSOR_ADDR, Max44009::Boolean::False);
        bool isOk;
        I2CMultiplexer* plexer;
        int busNum;
        bool isMultiplexer;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        }
    public: 
        Max44009Sensor(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus) {
            this->_wire = wire;
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        Max44009Sensor(TwoWire* wire) {
            this->_wire = wire;
            this->isMultiplexer = false;
        }
        void init() {
            this->myLux.configure(LUX_SENSOR_ADDR, this->_wire, Max44009::Boolean::False);
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            this->myLux.setManualMode(CDR, TIM);
            int err = this->myLux.getError();
            if (err != 0)
            {
                dbg.printf("failed to init lux sensor: %d\n", err);
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
            this->doPlex();
            float lux = myLux.getLux();
            dbg.printf("lux sensor returned value %f\r\n", lux);
            if (lux < 0) {
                dbg.printf("lux sensor read error, code: %f\r\n", lux);
                return NAN;
            }
            return lux;
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.readingCount = 1;
            reading.isComplete = true;
            reading.deferUntil = 0;
            this->doPlex();
            float lux = myLux.getLux();
            if (isnan(lux) || lux < 0) {
                dbg.printf("lux sensor read error, code: %f\r\n", lux);
                reading.values[0] = NAN;
                reading.isSuccessful = false;
            } else {
                reading.values[0] = lux;
                reading.isSuccessful = true;
            }
            return reading;            
        }

        void finishRead(DeferredReading &reading) {
            //it's instant, so nothing happens for finish
        }
};


#endif