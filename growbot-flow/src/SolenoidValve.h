#include <Arduino.h>
#include "DebugUtils.h"
#include "FlowValve.h"

#pragma once

class SolenoidValve : public FlowValve {
    private:
        byte pin;
        int openPulseMS;
        byte holdDutyCycle;
        bool turnedOn;
        static int instanceCounter;
        int ledcChannel;
        unsigned long switchToHoldStamp = 0;
    public:
        SolenoidValve(byte pin, int openPulseMS, byte holdDutyCycle) {
            this->pin = pin;
            this->openPulseMS = openPulseMS;
            this->holdDutyCycle = holdDutyCycle;
            ledcChannel = instanceCounter;
            
            ledcSetup(ledcChannel, 1500, 8);
            ledcAttachPin(pin, ledcChannel);
            instanceCounter++;
        }
        void update() {
            if (turnedOn && switchToHoldStamp > 0 && millis() >= switchToHoldStamp) {
                //  dbg.printf("switching port %d to 25pct pwm\n", i);
                dbg.printf("pin %d using ledc channel %d set to hold now %d\n", pin, ledcChannel, holdDutyCycle);
                ledcWrite(ledcChannel, holdDutyCycle);
                switchToHoldStamp = 0;
            } 
        }

        bool isOpen() {
            return turnedOn;
        }

        //returns true if the state changed
        bool setOpen(bool open) {
            
            if (open == turnedOn) {
                return false;
            }
            turnedOn = open;
            if (!turnedOn) {
                ledcWrite(ledcChannel, 0);
                dbg.printf("pin %d using ledc channel %d set to zero\n", pin, ledcChannel);
                switchToHoldStamp = 0;
            } else {
                ledcWrite(ledcChannel, 255);
                dbg.printf("pin %d using ledc channel %d set to 255\n", pin, ledcChannel);
                switchToHoldStamp = millis() + openPulseMS;
            }
            return true;
        }
};


int SolenoidValve::instanceCounter = 0;