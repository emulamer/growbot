#include <Arduino.h>
#include <GbMsg.h>

#pragma once

class DucterStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(DucterStatusMsg);}
        DucterStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        DucterStatusMsg(String nodeId, float insideOpenPercent, float outsideOpenPercent) : GbMsg(__FUNCTION__, nodeId) {
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
            
        }

        float insideOpenPercent() {
            if ((*this)["insideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["insideOpenPercent"].as<float>();
        }
        float outsideOpenPercent() {
            if ((*this)["outsideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["outsideOpenPercent"].as<float>();
        }
        virtual ~DucterStatusMsg() {}
};

class DucterSetDuctsMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(DucterSetDuctsMsg);}
        DucterSetDuctsMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        DucterSetDuctsMsg(String nodeId, float insideOpenPercent, float outsideOpenPercent) : GbMsg(__FUNCTION__, nodeId) {
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
        }

        float insideOpenPercent() {
            if ((*this)["insideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["insideOpenPercent"].as<float>();
        }
        float outsideOpenPercent() {
            if ((*this)["outsideOpenPercent"].isNull()) {
                return NAN;
            }
            return (*this)["outsideOpenPercent"].as<float>();
        }
        virtual ~DucterSetDuctsMsg() {}
};