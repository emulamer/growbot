#include <Arduino.h>
#include <GbMsg.h>

#pragma once


class TimeSourceMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(TimeSourceMsg);}
        TimeSourceMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        TimeSourceMsg(unsigned long unixSeconds, unsigned long long unixMilliseconds) : GbMsg(NAMEOF(TimeSourceMsg)) {
            (*this)["unixSeconds"] = unixSeconds;
            (*this)["unixMilliseconds"] = unixMilliseconds;
        }

        unsigned long unixSeconds() {
            if ((*this)["unixSeconds"].isNull()) {
                return 0;
            }
            return (*this)["unixSeconds"].as<unsigned long>();
        }
        unsigned long long unixMilliseconds() {
            if ((*this)["unixMilliseconds"].isNull()) {
                return 0;
            }
            return (*this)["unixMilliseconds"].as<unsigned long long>();
        }
        virtual ~TimeSourceMsg() {}
};