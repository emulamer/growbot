#include <Arduino.h>
// #include <OneWire.h>
// #include <DallasTemperature.h>

#include <DebugUtils.h>
#include "SensorBase.h"
#ifndef WATERTEMPSENSOR_H
#define WATERTEMPSENSOR_H
class WaterTempSensor : public SensorBase {
    public:
        virtual void init();
        virtual float read();
};

#ifdef ARDUINO_ARCH_RP2040

#endif
#ifdef ARDUINO_ARCH_ESP32

#endif
#endif