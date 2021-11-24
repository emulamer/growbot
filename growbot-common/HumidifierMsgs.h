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
        HumidifierSetMsg(String nodeId, int mode, float onHumidityPercent, float offHumidityPercent, String targetSensorName) : GbMsg(__FUNCTION__, nodeId) { 
            (*this)["mode"] = mode;
            (*this)["onHumidityPercent"] = onHumidityPercent;
            (*this)["offHumidityPercent"] = offHumidityPercent;
            (*this)["targetSensorName"] = targetSensorName;
        }
        HumidifierSetMsg(String msgType, String nodeId, int mode, float onHumidityPercent, float offHumidityPercent, String targetSensorName) : GbMsg(msgType, nodeId) { 
            (*this)["mode"] = mode;
            (*this)["onHumidityPercent"] = onHumidityPercent;
            (*this)["offHumidityPercent"] = offHumidityPercent;
            (*this)["targetSensorName"] = targetSensorName;
        }

        int mode() {
            return (*this)["mode"].as<int>();
        }
        float onHumidityPercent() {
            if ((*this)["onHumidityPercent"].isNull()) {
                return NAN;
            }
            return (*this)["onHumidityPercent"].as<float>();
        }
        float offHumidityPercent() {
            if ((*this)["offHumidityPercent"].isNull()) {
                return NAN;
            }
            return (*this)["offHumidityPercent"].as<float>();
        }
        String targetSensorName() {
            return (*this)["targetSensorName"].as<String>();
        }
        virtual ~HumidifierSetMsg() {}
};
class HumidifierStatusMsg : public HumidifierSetMsg {
    public:
        virtual String myType() {return NAMEOF(HumidifierStatusMsg);}
        HumidifierStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : HumidifierSetMsg(ref) {};
        HumidifierStatusMsg(String nodeId, int mode, float onHumidityPercent, float offHumidityPercent, String targetSensorName, bool isOn, float currentHumidityPercent) : HumidifierSetMsg(__FUNCTION__, nodeId, mode, onHumidityPercent, offHumidityPercent, targetSensorName) { 
            (*this)["currentHumidityPercent"] = currentHumidityPercent;
            (*this)["isOn"] = isOn;
        }
        bool isOn() {
            return (*this)["isOn"].as<bool>();
        }
        float currentHumidityPercent() {
            if ((*this)["currentHumidityPercent"].isNull()) {
                return NAN;
            }
            return (*this)["currentHumidityPercent"].as<float>();
        }
        virtual ~HumidifierStatusMsg() {}
};
