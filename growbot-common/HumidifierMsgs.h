#include <Arduino.h>
#include <GbMsg.h>

#pragma once

#define HUMIDIFIER_MODE_OFF 0
#define HUMIDIFIER_MODE_ON 1
#define HUMIDIFIER_MODE_AUTO 2
class HumidifierSetMsg : public GbMsg {
    public:
        virtual String myType() {return NAMEOF(HumidifierSetMsg);}
        HumidifierSetMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        HumidifierSetMsg(int mode, float onPct, float offPct, String sensor) : GbMsg(__FUNCTION__) { 
            (*this)["mode"] = mode;
            (*this)["onPct"] = onPct;
            (*this)["offPct"] = offPct;
            (*this)["sensor"] = sensor;
        }
        HumidifierSetMsg(String msgType, int mode, float onPct, float offPct, String sensor) : GbMsg(msgType) { 
            (*this)["mode"] = mode;
            (*this)["onPct"] = onPct;
            (*this)["offPct"] = offPct;
            (*this)["sensor"] = sensor;
        }

        int mode() {
            return (*this)["mode"].as<int>();
        }
        float onPct() {
            if ((*this)["onPct"].isNull()) {
                return NAN;
            }
            return (*this)["onPct"].as<float>();
        }
        float offPct() {
            if ((*this)["offPct"].isNull()) {
                return NAN;
            }
            return (*this)["offPct"].as<float>();
        }
        String sensor() {
            return (*this)["sensor"].as<String>();
        }
        virtual ~HumidifierSetMsg() {}
};
class HumidifierStatusMsg : public HumidifierSetMsg {
    public:
        virtual String myType() {return NAMEOF(HumidifierStatusMsg);}
        HumidifierStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : HumidifierSetMsg(ref) {};
        HumidifierStatusMsg(int mode, float onPct, float offPct, String sensor, bool isOn, float humPct) : HumidifierSetMsg(__FUNCTION__, mode, onPct, offPct, sensor) { 
            (*this)["humPct"] = humPct;
            (*this)["isOn"] = isOn;
        }
        bool isOn() {
            return (*this)["isOn"].as<bool>();
        }
        float humPct() {
            if ((*this)["humPct"].isNull()) {
                return NAN;
            }
            return (*this)["humPct"].as<float>();
        }
        virtual ~HumidifierStatusMsg() {}
};
