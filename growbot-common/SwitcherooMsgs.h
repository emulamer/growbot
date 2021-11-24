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
        SwitcherooStatusMsg(String nodeId, bool portStatus[6]) : GbMsg(__FUNCTION__, nodeId) {
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
        SwitcherooSetPortsMsg(String nodeId, SwitcherooPortStatus ports) : GbMsg(NAMEOF(SwitcherooSetPortsMsg), nodeId) {
            (*this)["ports"]["port0"] = ports.portStatus[0];
            (*this)["ports"]["port1"] = ports.portStatus[1];
            (*this)["ports"]["port2"] = ports.portStatus[2];
            (*this)["ports"]["port3"] = ports.portStatus[3];
            (*this)["ports"]["port4"] = ports.portStatus[4];
            (*this)["ports"]["port5"] = ports.portStatus[5];
        }
        virtual ~SwitcherooSetPortsMsg() {}

        SwitcherooPortStatus ports() {
            return {
                (*this)["ports"]["port0"].as<bool>(),
                (*this)["ports"]["port1"].as<bool>(),
                (*this)["ports"]["port2"].as<bool>(),
                (*this)["ports"]["port3"].as<bool>(),
                (*this)["ports"]["port4"].as<bool>(),
                (*this)["ports"]["port5"].as<bool>()
            };
        }
};