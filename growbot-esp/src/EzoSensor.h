#include <Arduino.h>
#include <Ezo_i2c.h> 
#include "GrowbotData.h"
#include "I2CMultiplexer.h"
#include "DebugUtils.h"
#ifndef EZOSENSOR_H
#define EZOSENSOR_H
#define EZO_BUFFER_LEN 41

#define EZO_CALIBRATE_LOW 1
#define EZO_CALIBRATE_MID 2
#define EZO_CALIBRATE_HIGH 3
#define EZO_CALIBRATE_DRY 5

class EzoSensor {
    protected:     
        Ezo_board* board;
        const char* name;
        bool isOk;
        I2CMultiplexer* plexer;
        byte busNum;
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
                dbg.printf("%s: sensor receive command failed with status code %d", this->name, status);
                return false;
            }
        }
        float readSensor(float waterTempC) {
            if (!this->isOk) {
                this->init();
            }
            if (!this->isOk) {
                return NAN;
            }
            bool readGood = false;
            for (byte i = 0; i < 3; i++) {
                this->doPlex();
                snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", waterTempC);
                this->board->send_cmd(this->buffer);
                delay(900);
                if (!this->recCheck()) {
                    dbg.printf("%s: read with temperature compensation failed!  retry %d", this->name, i);
                    dbg.println();
                    readGood = false;
                } else {
                    readGood = true;
                    break;
                }
            }
            dbg.printf("sensor %s responded with %s", this->name, this->buffer);
            float val = atof(this->buffer);
            return val;
        }
        bool calibrateSensor(byte point, float reference) {
            if (point != EZO_CALIBRATE_MID && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH && point != EZO_CALIBRATE_DRY) {
                return false;
            }
            this->doPlex();
            bool success = false;
            for (byte i = 0; i < 15; i++) {
                if (point == EZO_CALIBRATE_DRY) {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,dry");
                } else {
                    snprintf(this->buffer, EZO_BUFFER_LEN, "cal,%s,%f", (point == EZO_CALIBRATE_MID)?"mid":((point == EZO_CALIBRATE_LOW)?"low":((point == EZO_CALIBRATE_DRY)?"dry":"high")), reference);
                }
                
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
        virtual bool read(float waterTemperatureC, WaterData &output) = 0;
};



#endif