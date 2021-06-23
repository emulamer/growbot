#include <Arduino.h>
#include <SparkFun_SHTC3.h>
#include "I2CMultiplexer.h"
#include "GrowbotData.h"
#include <Adafruit_BME280.h>
#include <Wire.h>
#include "DebugUtils.h"

#ifndef TEMP_HUM_SENSOR_H
#define TEMP_HUM_SENSOR_H
#define SEALEVELPRESSURE_HPA (1013.25)
#define MAX_READ_FAIL_SKIP 4

class TempHumiditySensor : public DeferredSensorReader {
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
        int tempReadFailCount = 0;
        int humidReadFailCount = 0;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        } 
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
            doPlex();
            this->isOk = (this->bme.begin(0x76, &Wire) != 0);
            if (!this->isOk) {
                dbg.println("bme not ok");
            }
        }
        bool read(TempHumidity &output) {
            if (!this->isOk) {
                this->init();
            }
            if (this->isOk) {
                doPlex();
                float oldHum = output.humidity;
                float oldTemp = output.temperatureC;
                for (byte i = 1; i <= 3; i++) {
                    output.humidity = bme.readHumidity();
                    if (!isnan(output.humidity) && output.humidity > 0 && output.humidity < 100) {
                        break;
                    }
                    dbg.printf("Invalid BME280 humidity reading of %f, retry #%d\n", output.humidity, i);
                    delay(100);
                }
                if (!isnan(output.humidity) && output.humidity > 0 && output.humidity < 100) {
                    humidReadFailCount = 0;
                } else {
                    humidReadFailCount++;
                    if (humidReadFailCount < MAX_READ_FAIL_SKIP) {
                        output.humidity = oldHum;
                    } else {
                        dbg.println("BME280 humidity sensor has failed too many times to ignore!");
                        output.humidity = NAN;
                    }
                }
                
                for (byte i = 1; i <= 3; i++) {
                    output.temperatureC = bme.readTemperature();
                    if (!isnan(output.temperatureC) && output.temperatureC > 0 && output.temperatureC < 70) {
                        break;
                    }
                    dbg.printf("Invalid BME280 temp reading of %f, retry #%d\n", output.temperatureC, i);
                    delay(100);
                }
                if (!isnan(output.temperatureC) && output.temperatureC > 0 && output.temperatureC < 70) {
                    tempReadFailCount = 0;
                } else {
                    tempReadFailCount++;
                    if (tempReadFailCount < MAX_READ_FAIL_SKIP) {
                        output.temperatureC = oldTemp;
                    } else {
                        dbg.println("BME280 temp sensor has failed too many times to ignore!");
                        output.temperatureC = NAN;
                    }
                }
            } else {
                output.temperatureC = NAN;
                output.humidity = NAN;
            }
            return (!isnan(output.humidity) && !isnan(output.temperatureC));
        }
        DeferredReading startRead() {
            
            
            if (!this->isOk) {
                this->init();
            }
            
            DeferredReading reading;
            reading.sourceSensor = this;
            reading.deferUntil = 0;
            if (this->isOk) {
                this->doPlex();
                reading.values[0] = bme.readTemperature();
                reading.values[1] = bme.readHumidity();
                if (isnan(reading.values[0]) || isnan(reading.values[1]) || reading.values[0] == 0 || reading.values[1] == 0) {
                    reading.isSuccessful = false;
                    reading.values[0] = NAN;
                    reading.values[1] = NAN;
                } else {
                    reading.isSuccessful = true;
                }                
            } else {
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                reading.values[1] = NAN;
            }
            reading.isComplete = true;
            return reading;            
        }
        void finishRead(DeferredReading &reading) {
            //doesn't do anything since it isn't really deferred
        }
};

class SHTC3Sensor : public TempHumiditySensor {
    private:
        SHTC3 mySHTC3;
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
        bool isMultiplexer;
        int readFailCount = 0;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        }
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
                dbg.println("passIDcrc not ok");
            }
            if (this->isOk) {
                this->isOk = (mySHTC3.wake(true) == SHTC3_Status_Nominal);
            } else {
                dbg.println("wake not ok");
            }
        }
        bool read(TempHumidity &output) {
            if (this->isOk) {
                float oldHum = output.humidity;
                float oldTemp = output.temperatureC;
                if (this->isMultiplexer) {
                    this->plexer->setBus(this->busNum);
                }
                byte retries = 1;
                while (retries <= 3) {
                    this->mySHTC3.update();    
                    if(this->mySHTC3.lastStatus == SHTC3_Status_Nominal)
                    {
                        output.humidity = this->mySHTC3.toPercent();
                        output.temperatureC = this->mySHTC3.toDegC();
                        if (!isnan(output.humidity) && !isnan(output.temperatureC) && output.humidity > 0 && output.humidity < 100
                                && output.temperatureC > 0 && output.temperatureC < 70) 
                        {
                            break;
                        }
                        dbg.printf("SHT sensor failed read with temp %f hum %f, retry %d\n", output.temperatureC, output.humidity, retries);
                    } else {
                        dbg.printf("SHT sensor failed read with status %d, retry %d\n", static_cast<int>(this->mySHTC3.lastStatus), retries);
                    }
                    retries++;
                    delay(100);
                }
                if (!isnan(output.humidity) && !isnan(output.temperatureC) && output.humidity > 0 && output.humidity < 100
                                && output.temperatureC > 0 && output.temperatureC < 70)
                {
                    readFailCount = 0;
                } else {
                    readFailCount++;
                    if (readFailCount < MAX_READ_FAIL_SKIP) {
                        output.temperatureC = oldTemp;
                        output.humidity = oldHum;
                    } else {
                        dbg.println("SHT sensor has failed too many times to ignore!");
                        output.temperatureC = NAN;
                        output.humidity = NAN;
                    }
                }                
            } else {
                output.humidity = NAN;
                output.temperatureC = NAN;
            }
            return (!isnan(output.humidity) && !isnan(output.temperatureC));
        }
        DeferredReading startRead() {
            DeferredReading reading;
            reading.sourceSensor = this;
            reading.deferUntil = 0;
            this->doPlex();
            this->mySHTC3.update();    
            if(this->mySHTC3.lastStatus == SHTC3_Status_Nominal)
            {
                reading.values[0] = this->mySHTC3.toDegC();
                reading.values[1] = this->mySHTC3.toPercent();
                reading.isSuccessful = true;
            } else {
                dbg.printf("SHT sensor failed read with status %d\n", static_cast<int>(this->mySHTC3.lastStatus));
                reading.values[0] = NAN;
                reading.values[1] = NAN;
                reading.isSuccessful = false;
            }
            reading.isComplete = true;
            return reading;            
        }
        void finishRead(DeferredReading &reading) {
            //doesn't do anything since it isn't really deferred
        }
};

#endif