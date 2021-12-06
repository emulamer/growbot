#include <Arduino.h>
#include <GbMsg.h>

#pragma once

class LuxSensorStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(LuxSensorStatusMsg);}
        LuxSensorStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        LuxSensorStatusMsg(float lux) : GbMsg(__FUNCTION__) { 
            if (!isnan(lux)) {
                (*this)["lux"] = lux;
            }
        }

        float lux() {
            if ((*this)["lux"].isNull()) {
                return NAN;
            }
            return (*this)["lux"].as<float>();
        }
        virtual ~LuxSensorStatusMsg() {}
};