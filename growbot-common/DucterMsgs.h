#include <Arduino.h>
#include <GbMsg.h>

#pragma once
#define DUCTER_MODE_MANUAL 0
#define DUCTER_MODE_AUTO 1

class DucterStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(DucterStatusMsg);}
        DucterStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        DucterStatusMsg(int mode, float insideOpenPercent, float outsideOpenPercent, float targetTempC, String msgType = NAMEOF(DucterStatusMsg)) : GbMsg(msgType) {
            if (isnan(insideOpenPercent)) {
                (*this)["insideOpenPercent"] = NULL;
            } else {
                (*this)["insideOpenPercent"] = insideOpenPercent;
            }
            if (isnan(outsideOpenPercent)) {
                (*this)["outsideOpenPercent"] = NULL;
            } else {
                (*this)["outsideOpenPercent"] = outsideOpenPercent;
            }
            if (!isnan(targetTempC)) {
                (*this)["targetTempC"] = targetTempC;
            }
            
            (*this)["mode"] = mode;
        }

        int mode() {
            return (*this)["mode"].as<int>();
        }
        float insideOpenPercent() {
            if ((*this)["insideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["insideOpenPercent"].as<float>();
        }
        float targetTempC() {
            if ((*this)["targetTempC"].isNull()) {
                return NAN;
            }
            return (*this)["targetTempC"].as<float>();
        }
        float outsideOpenPercent() {
            if ((*this)["outsideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["outsideOpenPercent"].as<float>();
        }
        virtual ~DucterStatusMsg() {}
};

class DucterSetDuctsMsg : public DucterStatusMsg {
    public:
        String myType() {return NAMEOF(DucterSetDuctsMsg);}
        DucterSetDuctsMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : DucterStatusMsg(ref) {};
        DucterSetDuctsMsg(int mode, float insideOpenPercent, float outsideOpenPercent, float targetTempC) : DucterStatusMsg(mode, insideOpenPercent, outsideOpenPercent, targetTempC, __FUNCTION__) {
        }

        virtual ~DucterSetDuctsMsg() {}
};