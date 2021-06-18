
#include "EzoSensor.h"
#include "GrowbotData.h"

#ifndef CONDUCTIVITYSENSOR_H
#define CONDUCTIVITYSENSOR_H
class ConductivitySensor : public EzoSensor {
     public:
        ConductivitySensor(I2CMultiplexer* multiplexer, byte multiplexer_bus) : EzoSensor::EzoSensor(multiplexer, multiplexer_bus, "EC", 0x64){
        }
        ConductivitySensor() : EzoSensor::EzoSensor("EC", 0x64){
        }
        bool read(WaterData &output) {
            return this->read(25.0, output);
        } 
        bool read(float waterTemperatureC, WaterData &output) {
            float reading = this->readSensor(waterTemperatureC);
            output.tds = reading;
            return (reading != NAN);
        }   
        bool calibrate(byte point, float reference) {
            if (point != EZO_CALIBRATE_DRY && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH) {
                return false;
            }
            return this->calibrateSensor(point, reference);
        }
        void init() {
            this->EzoSensor::init();
            if (this->isOk) {
                //enable tds output
                this->doPlex();
                this->board->send_cmd("O,TDS,1");
                delay(300);
                if (this->recCheck()) {
                    Serial.print(this->name);
                    Serial.print(": sensor responded to TDS enable with: ");
                    Serial.println(this->buffer);
                    this->isOk = true;
                } else {
                    Serial.print(this->name);
                    Serial.print(": sensor failed to enable TDS");
                    this->isOk = false;
                }
                //disable EC output
                this->doPlex();
                this->board->send_cmd("O,EC,0");
                delay(300);
                if (this->recCheck()) {
                    Serial.print(this->name);
                    Serial.print(": sensor responded to EC disable with: ");
                    Serial.println(this->buffer);
                    this->isOk = true;
                } else {
                    Serial.print(this->name);
                    Serial.print(": sensor failed to disable EC");
                    this->isOk = false;
                }
            }
        }
};

#endif