#include <Arduino.h>
#include "DebugUtils.h"

#pragma once

class SolenoidValve {
    private:
        int openPulseMS;
        byte holdDutyCycle;
        bool turnedOn;
        static int instanceCounter;
        int ledcChannel;
        unsigned long switchToHoldStamp = 0;
    public:
        SolenoidValve(byte pin, int openPulseMS, byte holdDutyCycle) {
            this->openPulseMS = openPulseMS;
            this->holdDutyCycle = holdDutyCycle;
            ledcChannel = instanceCounter;
            ledcSetup(ledcChannel, 5000, 8);
            ledcAttachPin(pin, ledcChannel);
            instanceCounter++;
        }

        void update() {
            if (turnedOn && switchToHoldStamp > 0 && millis() < switchToHoldStamp) {
                //  dbg.printf("switching port %d to 25pct pwm\n", i);
                ledcWrite(ledcChannel, holdDutyCycle);
                switchToHoldStamp = 0;
            } 
        }

        bool isOn() {
            return turnedOn;
        }

        //returns true if the state changed
        bool setOn(bool on) {
            if (on == turnedOn) {
                return false;
            }
            turnedOn = on;
            if (!turnedOn) {
                ledcWrite(ledcChannel, 0);
                switchToHoldStamp = 0;
            } else {
                ledcWrite(ledcChannel, 255);
                switchToHoldStamp = millis() + openPulseMS;
            }
            return true;
        }
};


int SolenoidValve::instanceCounter = 0;