#include <Arduino.h>
#include <Ezo_i2c.h> 
#include "GrowbotData.h"
#include "I2CMultiplexer.h"

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
                Serial.print(this->name);
                Serial.print(": sensor receive command failed with status code ");
                Serial.println(status);
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
            this->doPlex();
            snprintf(this->buffer, EZO_BUFFER_LEN, "RT,%f", waterTempC);
            this->board->send_cmd(this->buffer);
            delay(900);
            if (!this->recCheck()) {
                Serial.print(this->name);
                Serial.println(": read with temperature compensation failed!");
                return NAN;
            }
            Serial.print("sensor ");
            Serial.print(this->name);
            Serial.print(" responded with ");
            Serial.println(this->buffer);
            float val = atof(this->buffer);
            return val;
        }
        bool calibrateSensor(byte point, float reference) {
            if (point != EZO_CALIBRATE_MID && point != EZO_CALIBRATE_LOW && point != EZO_CALIBRATE_HIGH && point != EZO_CALIBRATE_DRY) {
                return false;
            }
            this->doPlex();
            snprintf(this->buffer, EZO_BUFFER_LEN, "cal,%s,%f", (point == EZO_CALIBRATE_MID)?"mid":((point == EZO_CALIBRATE_LOW)?"low":((point == EZO_CALIBRATE_DRY)?"dry":"high")), reference);
            this->board->send_cmd(this->buffer);
            delay(900);
            if (!this->recCheck()) {
                Serial.println("set calibration point failed!");
                return false;
            }
            return true;
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
                Serial.print(this->name);
                Serial.print(": sensor responded with: ");
                Serial.println(this->buffer);
                this->isOk = true;
            } else {
                Serial.print(this->name);
                Serial.print(": sensor failed init");
                this->isOk = false;
            }
        }
        virtual bool read(float waterTemperatureC, WaterData &output) = 0;
};



#endif