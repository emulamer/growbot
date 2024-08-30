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
    DoserContinuousEndedMsg(int port, float actualMl) : GbMsg(NAMEOF(DoserContinuousEndedMsg)) {
        (*this)["port"] = port;
        (*this)["actualMl"] = actualMl;
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
};


class DoserCalibrateEndedMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserCalibrateEndedMsg);}
    DoserCalibrateEndedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserCalibrateEndedMsg(int port, bool success,  long bottle2SensorSteps, long sensor2DoseSteps, int errorCode, String errorMessage) : GbMsg(NAMEOF(DoserCalibrateEndedMsg)) {
        (*this)["port"] = port;
        (*this)["success"] = success;
        (*this)["bottle2SensorSteps"] = bottle2SensorSteps;
        (*this)["sensor2DoseSteps"] = sensor2DoseSteps;
        (*this)["errorCode"] = errorCode;
        (*this)["errorMessage"] = errorMessage;
    }

    int port() {
        return (*this)["port"].as<int>();
    }
    bool success() {
        if ((*this)["success"].isNull()) {
            return false;
        }
        return (*this)["success"].as<bool>();
    }
    long bottle2SensorSteps() {
        if ((*this)["bottle2SensorSteps"].isNull()) {
            return -1;
        }
        return (*this)["bottle2SensorSteps"].as<long>();
    }
    long sensor2DoseSteps() {
        if ((*this)["sensor2DoseSteps"].isNull()) {
            return -1;
        }
        return (*this)["sensor2DoseSteps"].as<long>();
    }
    int errorCode() {
        if ((*this)["errorCode"].isNull()) {
            return -1;
        }
        return (*this)["errorCode"].as<int>();
    }
    String errorMessage() {
        if ((*this)["errorMessage"].isNull()) {
            return "";
        }
        return (*this)["errorMessage"].as<String>();
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
    DoserEndedMsg(int port, float actualMl, bool success) : GbMsg(NAMEOF(DoserEndedMsg)) {
        (*this)["port"] = port;
        (*this)["actualMl"] = actualMl;
        (*this)["success"] = success;
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
};

class DoserEStopMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserEStopMsg);}
    DoserEStopMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserEStopMsg() : GbMsg(NAMEOF(DoserEStopMsg)) {

    }

};

class DoserRetractPortMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserRetractPortMsg);}
    DoserRetractPortMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserRetractPortMsg(int port) : GbMsg(NAMEOF(DoserRetractPortMsg)) {
        (*this)["port"] = port;
    }
    int port() {
        if ((*this)["port"].isNull()) {
            return -1;
        }
        return (*this)["port"].as<int>();
    }
};

class DoserCalibratePortMsg : public GbMsg {
    public:
    String myType() {return NAMEOF(DoserCalibratePortMsg);}
    DoserCalibratePortMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
    DoserCalibratePortMsg(int port, int startStep) : GbMsg(NAMEOF(DoserCalibratePortMsg)) {
        (*this)["port"] = port;
        (*this)["startStep"] = startStep;
    }

    int port() {
        if ((*this)["port"].isNull()) {
            return -1;
        }
        return (*this)["port"].as<int>();
    }
    int startStep() {
        if ((*this)["startStep"].isNull()) {
            return 0;
        }
        return (*this)["startStep"].as<int>();
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
    DoserStartContinuousPortMsg(int port) : GbMsg(NAMEOF(DoserStartContinuousPortMsg)) {
        (*this)["port"] = port;
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
