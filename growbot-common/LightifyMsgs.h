#include <Arduino.h>
#include <GbMsg.h>

#pragma once
#define LIGHTIFY_MODE_MANUAL 0
#define LIGHTIFY_MODE_AUTO 1
class LightifySetMsg : public GbMsg {
    public:
        virtual String myType() {return NAMEOF(LightifySetMsg);}
        LightifySetMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        LightifySetMsg(int mode, float lightPercent, float targetLux, String sensor, String msgType = NAMEOF(LightifySetMsg)) : GbMsg(msgType) { 
            (*this)["mode"] = mode;
            if (!isnan(lightPercent)) {
                (*this)["lightPercent"] = lightPercent;
            }
            if (!isnan(targetLux)) {
                (*this)["targetLux"] = targetLux;
            }
            (*this)["sensor"] = sensor;
        }

        float lightPercent() {
            if ((*this)["lightPercent"].isNull()) {
                return NAN;
            }
            return (*this)["lightPercent"].as<float>();
        }
        float targetLux() {
            if ((*this)["targetLux"].isNull()) {
                return NAN;
            }
            return (*this)["targetLux"].as<float>();
        }
        int mode() {
            return (*this)["mode"].as<int>();
        }
        String sensor() {
            return (*this)["sensor"].as<String>();
        }
        virtual ~LightifySetMsg() {}
};

class LightifyStatusMsg : public LightifySetMsg {
    public:
        virtual String myType() {return NAMEOF(LightifyStatusMsg);}
        LightifyStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : LightifySetMsg(ref) {};
        LightifyStatusMsg(int mode, float lightPercent, float targetLux, String sensor) : LightifySetMsg(mode, lightPercent, targetLux, sensor, NAMEOF(LightifyStatusMsg)) {
        }
};