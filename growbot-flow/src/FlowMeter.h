#include <Arduino.h>
#include "DebugUtils.h"

#pragma once

class FlowMeter {
    private:
        uint32_t pulseCount = 0;
        uint32_t ppl = 0;
        int pin;
        static void IRAM_ATTR pulseCounter(void* ref);
    public:
        FlowMeter(byte inputPin, int pulsesPerLiter) {
            this->pin = inputPin;
            this->ppl = pulsesPerLiter;

            //((float)ticks/(float)60) * 0.264172F
            attachInterruptArg(this->pin, FlowMeter::pulseCounter, this, FALLING);
        }
        float getLitersSinceReset() {
            dbg.printf("ticks: %d\n", this->pulseCount);
            float gals = ((float)pulseCount)/ ((float)ppl);
            return gals;
        }

        void resetFrom(float liters) {
            uint32_t pulses = (this->ppl * liters);
            if (this->pulseCount >= pulses) {
                this->pulseCount -= pulses;
            } else {
                this->pulseCount = 0;
            }
        }
};

void IRAM_ATTR FlowMeter::pulseCounter(void* ref) {
    FlowMeter* flowmeter = (FlowMeter*)ref;
    flowmeter->pulseCount++;
}