#include <Arduino.h>
#include <Ezo_i2c.h> 
#include "GrowbotData.h"
#include "I2CMultiplexer.h"
#include "DebugUtils.h"
#include "DeferredSensorReader.h"

#ifndef EZOSENSOR_H
#define EZOSENSOR_H
#define EZO_BUFFER_LEN 41

#define EZO_CALIBRATE_LOW 1
#define EZO_CALIBRATE_MID 2
#define EZO_CALIBRATE_HIGH 3
#define EZO_CALIBRATE_DRY 5

class EzoSensor : public DeferredSensorReader {
    protected:     
        Ezo_board* board;
        const char* name;
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
        float waterTempC;
        bool isMultiplexer;
        char* buffer;
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
                dbg.printf("%s: sensor receive command failed with status code %d\n", this->name, status);
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
                snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", this->waterTempC);
                this->board->send_cmd(this->buffer);
                delay(900);
                if (!this->recCheck()) {
                    dbg.printf("%s: read with temperature compensation failed!  retry %d\n", this->name, i);
                    dbg.println();
                } else {
                    break;
                }
            }
            dbg.printf("sensor %s responded with %s\n", this->name, this->buffer);
            float val = atof(this->buffer);
            return val;
        }
        bool calibrateSensor(byte point, float reference) {
            if (point != EZO_CALIBRATE_MID && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH && point != EZO_CALIBRATE_DRY) {
                return false;
            }
            
            bool success = false;
            for (byte i = 0; i < 15; i++) {
                if (point == EZO_CALIBRATE_DRY) {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,dry");
                } else {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,%s,%f", (point == EZO_CALIBRATE_MID)?"mid":((point == EZO_CALIBRATE_LOW)?"low":((point == EZO_CALIBRATE_DRY)?"dry":"high")), reference);
                }
                this->doPlex();    
                this->board->send_cmd(this->buffer);
                delay(900);
                if (!this->recCheck()) {
                    dbg.println("set calibration point failed!  Trying again...");
                } else {
                    success = true;
                    break;
                }
            }
            return success;
        }
        
    public:
        EzoSensor(I2CMultiplexer* multiplexer, byte multiplexer_bus, const char* boardName, int boardAddress) {
            this->name = boardName;
            this->board = new Ezo_board(boardAddress, boardName);
            this->buffer = (char *)malloc(EZO_BUFFER_LEN);
            this->isMultiplexer = true;
            this->plexer = multiplexer;
            this->busNum = multiplexer_bus;            
        } 
        EzoSensor(const char* boardName, int boardAddress) {
            this->name = boardName;
            this->board = new Ezo_board(boardAddress, boardName);
            this->buffer = (char *)malloc(EZO_BUFFER_LEN);
            this->isMultiplexer = false;            
        }

        virtual void init() {
            this->doPlex();
            this->board->send_cmd("Status");
            delay(300);
            if (this->recCheck()) {
                dbg.printf("%s: sensor responded with: %s\n", this->name, this->buffer);
                this->isOk = true;
            } else {
                dbg.printf("%s: sensor failed init\n", this->name);
                this->isOk = false;
            }
        }
        virtual bool read(WaterData &output) = 0;

        void setWaterTempCompensation(float tempC) {
            if (isnan(tempC) || tempC < 0 || tempC > 50) {
                dbg.printf("Invalid temp provided for compensation: %f\n", tempC);
                this->waterTempC = 25;
            } else {
                this->waterTempC = tempC;
            }
        }
        virtual DeferredReading startRead() {
            DeferredReading reading;            
            reading.sourceSensor = this;
            reading.isComplete = false;
            reading.readingCount = 1;
            reading.isSuccessful = false;
            reading.values[0] = NAN;
            reading.deferUntil = millis() + 1000;
            snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", this->waterTempC);
            this->doPlex();
            this->board->send_cmd(this->buffer);
            return reading;
        }
        virtual void finishRead(DeferredReading &reading) {
            if (!this->recCheck()) {
                dbg.printf("%s: read with temperature compensation failed!\n", this->name);
                reading.isSuccessful = false;
                reading.values[0] = NAN;
            } else {
                reading.values[0] = atof(this->buffer);
                reading.isSuccessful = true;
            }
            dbg.printf("sensor %s responded with %s\n", this->name, this->buffer);
            reading.isComplete = true;
        }
};



#endif