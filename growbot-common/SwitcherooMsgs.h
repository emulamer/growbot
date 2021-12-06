#include <Arduino.h>
#include <GbMsg.h>

#pragma once

struct SwitcherooPortStatus {
    bool portStatus[6];
};

class SwitcherooStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(SwitcherooStatusMsg);}
        SwitcherooStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        SwitcherooStatusMsg(bool portStatus[6]) : GbMsg(__FUNCTION__) {
            (*this)["status"]["ports"]["port0"] = portStatus[0];
            (*this)["status"]["ports"]["port1"] = portStatus[1];
            (*this)["status"]["ports"]["port2"] = portStatus[2];
            (*this)["status"]["ports"]["port3"] = portStatus[3];
            (*this)["status"]["ports"]["port4"] = portStatus[4];
            (*this)["status"]["ports"]["port5"] = portStatus[5];
        }
        virtual ~SwitcherooStatusMsg() {}

        SwitcherooPortStatus ports() {
            return {(*this)["status"]["ports"]["port0"].as<bool>(),
                    (*this)["status"]["ports"]["port1"].as<bool>(),
                    (*this)["status"]["ports"]["port2"].as<bool>(),
                    (*this)["status"]["ports"]["port3"].as<bool>(),
                    (*this)["status"]["ports"]["port4"].as<bool>(),
                    (*this)["status"]["ports"]["port5"].as<bool>()};
        }
};

class SwitcherooSetPortsMsg : public GbMsg {
    public:
        String myType() { return NAMEOF(SwitcherooSetPortsMsg);}
        SwitcherooSetPortsMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {}
        SwitcherooSetPortsMsg() : GbMsg(NAMEOF(SwitcherooSetPortsMsg)) {
        }
        virtual ~SwitcherooSetPortsMsg() {}

        bool hasPort(int portNum) {
            return (*this)["ports"].containsKey(String("port")+String(portNum));
        }
        bool getPort(int portNum) {
            return (*this)["ports"][String("port")+String(portNum)];
        }
        void setPort(int portNum, bool isOn) {
            (*this)["ports"][String("port")+String(portNum)] = isOn;
        }
};