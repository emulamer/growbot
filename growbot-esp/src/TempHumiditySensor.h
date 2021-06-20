#include <Arduino.h>
#include <SparkFun_SHTC3.h>
#include "I2CMultiplexer.h"
#include "GrowbotData.h"
#include <Adafruit_BME280.h>
#include <Wire.h>
//needed? #include <Adafruit_Sensor.h>
#ifndef TEMP_HUM_SENSOR_H
#define TEMP_HUM_SENSOR_H
#define SEALEVELPRESSURE_HPA (1013.25)

class TempHumiditySensor {
    public:
        virtual void init() = 0;
        virtual bool read(TempHumidity &output) = 0;
};
 
class BME280Sensor : public TempHumiditySensor {
    private:     
        Adafruit_BME280 bme;
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
        bool isMultiplexer;
    public:
        BME280Sensor(I2CMultiplexer* multiplexer, byte multiplexer_bus) {
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        BME280Sensor() {
            this->isMultiplexer = false;
        }
        void init() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            this->isOk = (this->bme.begin(0x76, &Wire) != 0);
            if (!this->isOk) {
                Serial.println("bme not ok");
            }
        }
        bool read(TempHumidity &output) {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            if (!this->isOk) {
                this->isOk = (this->bme.begin() != 0);
            }
            if (this->isOk) {
                output.humidity = bme.readHumidity();
                output.temperatureC = bme.readTemperature();
                return true;
            }
            output.humidity = NAN;
            output.temperatureC = NAN;
            return false;
        }
};

class SHTC3Sensor : public TempHumiditySensor {
    private:
        SHTC3 mySHTC3;
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
        bool isMultiplexer;

    public:
        SHTC3Sensor(I2CMultiplexer* multiplexer, byte multiplexer_bus) {
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;
        }
        SHTC3Sensor() {
            this->isMultiplexer = false;
        }
        void init() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
            mySHTC3.begin(Wire);
            this->isOk = this->mySHTC3.passIDcrc;
            if (this->isOk) {
                this->isOk = (mySHTC3.setMode(SHTC3_CMD_CSE_TF_NPM) == SHTC3_Status_Nominal);
            } else {
                Serial.println("passIDcrc not ok");
            }
            if (this->isOk) {
                this->isOk = (mySHTC3.wake(true) == SHTC3_Status_Nominal);
            } else {
                Serial.println("wake not ok");
            }
        }
        bool read(TempHumidity &output) {
            if (this->isOk) {
                if (this->isMultiplexer) {
                    this->plexer->setBus(this->busNum);
                }
               this->mySHTC3.update();    
               if(this->mySHTC3.lastStatus == SHTC3_Status_Nominal)
               {
                    output.humidity = this->mySHTC3.toPercent();
                    output.temperatureC = this->mySHTC3.toDegC();
                    return true;
               } 
            }  else {
                Serial.println("not ok when read");
            }
            output.humidity = NAN;
            output.temperatureC = NAN;
            return false;
        }
};

#endif