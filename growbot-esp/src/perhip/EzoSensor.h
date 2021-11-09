#include <Arduino.h>
#include <Ezo_i2c.h> 
#include "../GrowbotData.h"
#include "I2CMultiplexer.h"
#include <DebugUtils.h>
#include "../SensorBase.h"

#ifndef EZOSENSOR_H
#define EZOSENSOR_H
#define EZO_BUFFER_LEN 45

#define EZO_CALIBRATE_LOW 1
#define EZO_CALIBRATE_MID 2
#define EZO_CALIBRATE_HIGH 3
#define EZO_CALIBRATE_DRY 5

class EzoSensor : public SensorBase {
    protected:     
        Ezo_board* board;
        const char* name;
        bool isOk;
        I2CMultiplexer* plexer;
        int busNum;
        float waterTempC;
        bool isMultiplexer;
        char* buffer;
        byte enPin;
        float lastGoodTempCalibration = NAN;
        void doPlex() {
            if (this->isMultiplexer) {
                this->plexer->setBus(this->busNum);
            }
        } 
        bool recCheck() {
            this->doPlex();

            auto status = board->receive_cmd(this->buffer, EZO_BUFFER_LEN);
            if (status == Ezo_board::errors::SUCCESS) {
                return true;
            } else {
                dbg.eprintf("%s: sensor receive command failed with status code %d, buffer is %s\n", this->name, status, this->buffer);
                return false;
            }
        }
        float readSensor() {
            if (!this->isOk) {
                this->init();
            }
            if (!this->isOk) {
                return NAN;
            }
            for (byte i = 0; i < 3; i++) {
                this->doPlex();
                memset(this->buffer, 0, EZO_BUFFER_LEN);
                snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", this->waterTempC);
                this->board->send_cmd(this->buffer);
                delay(1000);
                if (!this->recCheck()) {
                    dbg.eprintf("%s: read with temperature compensation failed on retry %d! Response: %s\n", this->name, i, this->buffer);
                } else {
                    break;
                }
            }
            //dbg.printf("Sensor %s responded with %s\n", this->name, this->buffer);
            float val = atof(this->buffer);
            
            return val;
        }
        bool calibrateSensor(byte point, float reference) {
            if (point != EZO_CALIBRATE_MID && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH && point != EZO_CALIBRATE_DRY) {
                return false;
            }
            
            bool success = false;
            for (byte i = 0; i < 15; i++) {
                memset(this->buffer, 0, EZO_BUFFER_LEN);
                if (point == EZO_CALIBRATE_DRY) {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,dry");
                } else {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,%s,%f", (point == EZO_CALIBRATE_MID)?"mid":((point == EZO_CALIBRATE_LOW)?"low":((point == EZO_CALIBRATE_DRY)?"dry":"high")), reference);
                }
                this->doPlex();    
                this->board->send_cmd(this->buffer);
                delay(1000);
                if (!this->recCheck()) {
                    dbg.eprintln("set calibration point failed!  Trying again...");
                } else {
                    success = true;
                    break;
                }
            }
            return success;
        }
        
    public:
        EzoSensor(TwoWire* wire, I2CMultiplexer* multiplexer, int multiplexer_bus, const char* boardName, int boardAddress, byte enablePin) {
            this->name = boardName;
            this->enPin = enablePin;
            this->board = new Ezo_board(boardAddress, wire);
            this->buffer = (char *)malloc(EZO_BUFFER_LEN);
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;            
        } 
        EzoSensor(TwoWire* wire, const char* boardName, int boardAddress, byte enablePin) {
            this->name = boardName;
            this->enPin = enablePin;
            this->board = new Ezo_board(boardAddress, wire);
            this->buffer = (char *)malloc(EZO_BUFFER_LEN);
            this->isMultiplexer = false;            
        }

        virtual void init() {
            pinMode(this->enPin, OUTPUT);
            digitalWrite(this->enPin, 0);
            delay(1000);
            digitalWrite(this->enPin, 1);
            delay(1000);
            this->doPlex();
            this->board->send_cmd("Status");
            delay(400);
            if (this->recCheck()) {
                dbg.dprintf("%s: sensor passed status check and responded with: %s\n", this->name, this->buffer);
                this->isOk = true;
            } else {
                dbg.eprintf("%s: sensor failed init\n", this->name);
                this->isOk = false;
            }
        }
        virtual bool read(WaterData &output) = 0;
        void preupdate(GrowbotData* data) {
            this->setWaterTempCompensation(data->controlBucket.temperatureC);
        }
        void setWaterTempCompensation(float tempC) {
            if (isnan(tempC) || tempC <= 0 || tempC > 50) {
                dbg.wprintf("Invalid temp provided for compensation: %f\n", tempC);
                if (!isnan(lastGoodTempCalibration)) {
                    dbg.wprintf("Using last known good water temp value of: %f\n", lastGoodTempCalibration);
                    this->waterTempC = lastGoodTempCalibration;
                } else {
                    this->waterTempC = 25;
                }
            } else {
                this->waterTempC = tempC;
                lastGoodTempCalibration = tempC;
            }
            dbg.dprintf("Using temp provided for compensation: %f\n", tempC);
        }
        virtual DeferredReading startRead() {
            if (!this->isOk) {
                this->init();
            }
            DeferredReading reading;            
            reading.isComplete = false;
            reading.readingCount = 1;
            reading.isSuccessful = false;
            reading.values[0] = NAN;
            reading.deferUntil = millis() + 1000;
            memset(this->buffer, 0, EZO_BUFFER_LEN);
            snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", this->waterTempC);
            this->doPlex();
            this->board->send_cmd(this->buffer);
            return reading;
        }
        virtual void finishRead(DeferredReading &reading) {
            if (!this->recCheck()) {
                dbg.eprintf("%s: read with temperature compensation failed!\n", this->name);
                reading.isSuccessful = false;
                reading.values[0] = NAN;
            } else {
                reading.values[0] = atof(this->buffer);
                reading.isSuccessful = true;
            }
            dbg.dprintf("sensor %s responded with %s\n", this->name, this->buffer);
            reading.isComplete = true;
        }
};



#endif