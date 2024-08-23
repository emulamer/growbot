#include <Arduino.h>
#include <GbMsg.h>

#pragma once
class PowerMonStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(PowerMonStatusMsg);}
        PowerMonStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        PowerMonStatusMsg(bool operative, float instantAmpDraw, double ampHoursSinceReset, unsigned long lastResetUnixSeconds) : GbMsg(NAMEOF(PowerMonStatusMsg)) {
            if (isnan(instantAmpDraw)) {
                (*this)["instantAmpDraw"] = NULL;
            } else {
                (*this)["instantAmpDraw"] = instantAmpDraw;
            }
            (*this)["operative"] = operative;
            (*this)["ampHoursSinceReset"] = ampHoursSinceReset;
            (*this)["lastResetUnixSeconds"] = lastResetUnixSeconds;
          
        }

        bool operative() {
            if ((*this)["operative"].isNull()) {
                return false;
            }
            return (*this)["operative"].as<bool>();
        }

        float instantAmpDraw() {
            if ((*this)["instantAmpDraw"].isNull()) {
                return NAN;
            }
            return (*this)["instantAmpDraw"].as<float>();
        }
            
        double ampHoursSinceReset() {
            if ((*this)["ampHoursSinceReset"].isNull()) {
                return 0;
            }
            
            return (*this)["ampHoursSinceReset"].as<double>();           
        }

        unsigned long lastResetUnixSeconds() {
            if ((*this)["lastResetUnixSeconds"].isNull()) {
                return 0;
            }
            return (*this)["lastResetUnixSeconds"].as<unsigned long>();
        }

        virtual ~PowerMonStatusMsg() {}
};


class PowerMonCommandMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(PowerMonCommandMsg);}
        PowerMonCommandMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        PowerMonCommandMsg(unsigned long unixSecondsStamp, bool resetAmpHours) : GbMsg(NAMEOF(PowerMonCommandMsg)) {
                (*this)["resetAmpHours"] = resetAmpHours;
         
        }

        bool resetAmpHours() {
            if ((*this)["resetAmpHours"].isNull()) {
                return false;
            }
            return (*this)["resetAmpHours"].as<bool>();
            
        }

        unsigned long unixSecondsStamp() {
            if ((*this)["unixSecondsStamp"].isNull()) {
                return 0;
            }
            return (*this)["unixSecondsStamp"].as<unsigned long>();
        }

    virtual ~PowerMonCommandMsg() {}
};