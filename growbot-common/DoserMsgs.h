#include <Arduino.h>
#include <GbMsg.h>

#pragma once


class DoserStartedMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserStartedMsg);}
    DoserStartedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserStartedMsg(int port, float targetMl) : GbMsg(NAMEOF(DoserStartedMsg)) {
        (*this)["port"] = port;
        (*this)["targetMl"] = targetMl;
    }

    int port() {
        return (*this)["port"].as<int>();
    }

    float targetMl() {
        if ((*this)["targetMl"].isNull()) {
            return NAN;
            return (*this)["targetMl"].as<float>();
        }
    }
};

class DoserContinuousStartedMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserPrimeStartedMsg);}
    DoserContinuousStartedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserContinuousStartedMsg(int port) : GbMsg(NAMEOF(DoserContinuousStartedMsg)) {
        (*this)["port"] = port;
    }

    int port() {
        return (*this)["port"].as<int>();
    }
};

class DoserContinuousEndedMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserContinuousEndedMsg);}
    DoserContinuousEndedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserContinuousEndedMsg(int port, float actualMl, bool isRetracted) : GbMsg(NAMEOF(DoserContinuousEndedMsg)) {
        (*this)["port"] = port;
        (*this)["actualMl"] = actualMl;
        (*this)["isRetracted"] = isRetracted;
    }

    int port() {
        return (*this)["port"].as<int>();
    }

    float actualMl() {
        if ((*this)["actualMl"].isNull()) {
            return NAN;
            return (*this)["actualMl"].as<float>();
        }
    }

    bool isRetracted() {
        if ((*this)["isRetracted"].isNull()) {
            return false;
        }
        return (*this)["isRetracted"].as<bool>();
    }
};

//not really much in this one right now, just adding a status message so it can say it's awake
class DoserStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(DoserStatusMsg);}
        DoserStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        DoserStatusMsg(bool isDosing ) : GbMsg(NAMEOF(DoserStatusMsg)) {
            (*this)["isDosing"] = isDosing;
        }

        bool mode() {
            return (*this)["isDosing"].as<bool>();
        }
        virtual ~DoserStatusMsg() {}
};


class DoserEndedMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserEndedMsg);}
    DoserEndedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserEndedMsg(int port, float actualMl, bool success, bool isRetracted) : GbMsg(NAMEOF(DoserEndedMsg)) {
        (*this)["port"] = port;
        (*this)["actualMl"] = actualMl;
        (*this)["success"] = success;
        (*this)["isRetracted"] = isRetracted;
    }

    int port() {
        return (*this)["port"].as<int>();
    }

    float actualMl() {
        if ((*this)["actualMl"].isNull()) {
            return NAN;
            return (*this)["actualMl"].as<float>();
        }
    }

    bool success() {
        if ((*this)["success"].isNull()) {
            return false;
        }
        return (*this)["success"].as<bool>();
    }

    bool isRetracted() {
        if ((*this)["isRetracted"].isNull()) {
            return false;
        }
        return (*this)["isRetracted"].as<bool>();
    }

};

class DoserEStopMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserEStopMsg);}
    DoserEStopMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserEStopMsg(bool skipRetract) : GbMsg(NAMEOF(DoserEStopMsg)) {
        (*this)["skipRetract"] = skipRetract;
    }

    bool skipRetract() {
        if ((*this)["skipRetract"].isNull()) {
            return false;
        }
        return (*this)["skipRetract"].as<bool>();
    }
};


class DoserDosePortMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserDosePortMsg);}
    DoserDosePortMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserDosePortMsg(int port, float targetMl) : GbMsg(NAMEOF(DoserDosePortMsg)) {
        (*this)["port"] = port;
        (*this)["targetMl"] = targetMl;
    }

    int port() {
        return (*this)["port"].as<int>();
    }

    float targetMl() {
        if ((*this)["targetMl"].isNull()) {
            return NAN;
        }
        return (*this)["targetMl"].as<float>();
    }
};


class DoserStartContinuousPortMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserStartContinuousPortMsg);}
    DoserStartContinuousPortMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserStartContinuousPortMsg(int port, bool skipRetract) : GbMsg(NAMEOF(DoserStartContinuousPortMsg)) {
        (*this)["port"] = port;
        (*this)["skipRetract"] = skipRetract;
    }
    bool skipRetract() {
        if ((*this)["skipRetract"].isNull()) {
            return false;
        }
        return (*this)["skipRetract"].as<bool>();
    }
    int port() {
        return (*this)["port"].as<int>();
    }
};

//this is going to be a bit of a pain because it's all metered and step counting.  let's see if we can get away with just using ML
// class DoserDosePortForMillisecMsg : public GbMsg {
//     public:
//     String myType() {return NAMEOF(DoserDosePortForMillisecMsg);}
//     DoserDosePortForMillisecMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
//     DoserDosePortForMillisecMsg(int port, float targetMs) : GbMsg(NAMEOF(DoserDosePortForMillisecMsg)) {
//         (*this)["port"] = port;
//         (*this)["targetMs"] = targetMs;
//     }

//     int port() {
//         return (*this)["port"].as<int>();
//     }

//     float targetMs() {
//         if ((*this)["targetMs"].isNull()) {
//             return NAN;
//             return (*this)["targetMs"].as<int>();
//         }
//     }
// };
