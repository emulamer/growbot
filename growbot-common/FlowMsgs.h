#include <Arduino.h>
#include "GbMsg.h"

#pragma once
struct FlowInfo {
    float litersIn = NAN;
    float litersOut = NAN;
    bool inValveOpen = false;
    bool outValveOpen = false;
    float waterLevel = NAN;
    void* currentOperation = NULL;
};
class FlowResetCounterMsg: public GbMsg {
    public:
        String myType() { return NAMEOF(FlowResetCounterMsg); }
        FlowResetCounterMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) { }
        FlowResetCounterMsg(String nodeId, String counterName, float fromAmount) : GbMsg(NAMEOF(FlowResetCounterMsg), nodeId) {
            (*this)["counterName"] = counterName;
            (*this)["fromAmount"] = fromAmount;
        }

        float fromAmount() {
            return (*this)["fromAmount"].as<float>();
        }

        String counterName() {
            return (*this)["counterName"].as<String>();
        }
};
class FlowStartOpMsg : public GbMsg {
    public:
        String myType() { return NAMEOF(FlowStartOpMsg); }
        FlowStartOpMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) { }
        FlowStartOpMsg(String nodeId, String opName, const std::initializer_list<float> &params) : GbMsg(NAMEOF(FlowStatusMsg), nodeId) {
            (*this)["op"] = opName;
            for (auto p: params) {
                (*this)["params"].add(p);
            }            
        }
        virtual ~FlowStartOpMsg() {}

        String op() {
            return (*this)["op"].as<String>();
        }
        int paramCount() {
            return (*this)["params"].as<JsonArray>().size();
        }
        float param(int index) {
            return (*this)["params"][index].as<float>();
        }

};

class FlowAbortOpMsg : public GbMsg {
    public:
        String myType() { return NAMEOF(FlowAbortOpMsg); }
        FlowAbortOpMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) { }
        FlowAbortOpMsg(String nodeId) : GbMsg(NAMEOF(FlowAbortOpMsg), nodeId) {
        }
        virtual ~FlowAbortOpMsg() {}
};


class FlowStatusMsg : public GbMsg {
    private:
        //in growbot-flow's main.cpp
        void setupMsg(FlowInfo& status);
    public:
        virtual String myType() { return NAMEOF(FlowStatusMsg); }
        FlowStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {}
        FlowStatusMsg(String nodeId, FlowInfo& status) : GbMsg(NAMEOF(FlowStatusMsg), nodeId) {
            setupMsg(status);
        }
        FlowStatusMsg(String msgType, String nodeId, FlowInfo& status) : GbMsg(msgType, nodeId) {
            setupMsg(status);
        }
        virtual ~FlowStatusMsg() {}
        float litersIn() {
            if (!(*this)["status"].containsKey("litersIn")) {
                return NAN;
            }
            return (*this)["status"]["litersIn"].as<float>();
        }
        float litersOut() {
            if ((*this)["status"]["litersOut"].isNull()) {
                return NAN;
            }
            return (*this)["status"]["litersOut"].as<float>();
        }
        float waterLevel() {
            if ((*this)["status"]["waterLevel"].isNull()) {
                return NAN;
            }
            return (*this)["status"]["waterLevel"].as<float>();
        }
        bool inValveOpen() {
            return (*this)["status"]["inValveOpen"].as<bool>();
        }
        bool outValveOpen() {
            return (*this)["status"]["outValveOpen"].as<bool>();
        }
        String currentOp() {
            if ((*this)["status"]["currentOp"].isNull()) {
                return "None";
            }
            return (*this)["status"]["currentOp"]["type"].as<String>();
        }
        String currentOpStatus() {
            if ((*this)["status"]["currentOp"].isNull()) {
                return "";
            }
            return (*this)["status"]["currentOp"]["status"].as<String>();
        }
        float currentOpLitersIn() {
            if ((*this)["status"]["currentOp"].isNull()) {
                return NAN;
            }
            return (*this)["status"]["currentOp"]["litersIn"].as<float>();
        }
        float currentOpLitersOut() {
            if ((*this)["status"]["currentOp"].isNull()) {
                return NAN;
            }
            return (*this)["status"]["currentOp"]["litersOut"].as<float>();
        }
        float currentOpLitersDelta() {
            if ((*this)["status"]["currentOp"].isNull()) {
                return NAN;
            }
            return (*this)["status"]["currentOp"]["litersDelta"].as<float>();
        }
        int currentOpParamCount() {
            return (*this)["params"].as<JsonArray>().size();
        }
        float currentOpParam(int index) {
            return (*this)["params"][index].as<float>();
        }
        String currentOpErrorMessage() {
            return (*this)["status"]["currentOp"]["errorMessage"].as<String>();
        }
        String currentOpId() {
            return (*this)["status"]["currentOp"]["id"].as<String>();
        }
};

class FlowOpStartedMsg : public FlowStatusMsg {
    public:
        virtual String myType() { return NAMEOF(FlowOpStartedMsg); }
        FlowOpStartedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : FlowStatusMsg(ref) { }
        FlowOpStartedMsg(String nodeId, FlowInfo& status) : FlowStatusMsg(NAMEOF(FlowOpStartedMsg), nodeId, status) {
        }
        ~FlowOpStartedMsg() {}
};

class FlowOpEndedMsg : public FlowStatusMsg {
    public:
        virtual String myType() { return NAMEOF(FlowOpEndedMsg); }
        FlowOpEndedMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : FlowStatusMsg(ref) { }
        FlowOpEndedMsg(String nodeId, FlowInfo& status) : FlowStatusMsg(NAMEOF(FlowOpEndedMsg), nodeId, status) {
        }
        ~FlowOpEndedMsg() {}
       
};