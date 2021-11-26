#include <Arduino.h>
#include <ArduinoJson.h>

#pragma once
#define MSG_JSON_SIZE 1024

#define NAMEOF(name) #name

class GbMsg : public StaticJsonDocument<MSG_JSON_SIZE> {
    public:
        GbMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : StaticJsonDocument<MSG_JSON_SIZE>(ref) {
        }

        GbMsg(String msgType, String nodeId) : StaticJsonDocument<MSG_JSON_SIZE>() {
            (*this)["msgId"] = String(random(10000000));
            (*this)["msgType"] = msgType;
            (*this)["nodeType"] = GB_NODE_TYPE;
            nodeId.replace(":","");
            (*this)["nodeId"] = MDNS_NAME;
        }
        virtual ~GbMsg() {};
        String msgId() {
            return (*this)["msgId"].as<String>();
        }
        String msgType() {
            return (*this)["msgType"].as<String>();
        }
        String nodeId() {
            return (*this)["nodeId"].as<String>();
        }
        String nodeType() {
            return (*this)["nodeType"].as<String>();
        }
        void setReplyToMsgId(String replyToMsgId) {
            replyToMsgId = replyToMsgId;
        }
        virtual String myType() = 0;
        String toJson() {
            String jsonStr;
            if (serializeJson((StaticJsonDocument<MSG_JSON_SIZE>)(*this), jsonStr) == 0) {
                dbg.printf("Failed to serialize reply json from %s\n", myType());
                return "";
            }
            return jsonStr;
        }
};
class GbResultMsg : public GbMsg {
    public:
        String myType() { return NAMEOF(GbResultMsg);}
        GbResultMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {}
        GbResultMsg(String msgType, String nodeId) : GbMsg(msgType, nodeId) {
        }
        GbResultMsg(String nodeId) : GbMsg(NAMEOF(GbResultMsg), nodeId) {
        }
        virtual ~GbResultMsg() {};
        void setSuccess() {
            (*this)["success"] = true;
        }
        void setUnsuccess(String error) {
            (*this)["success"] = false;
            (*this)["error"] = error;
        }
        bool isSuccess() {
            return (*this)["success"].as<bool>();
        }
        String getError() {
            return (*this)["error"].as<String>();
        }
};
class GbGetStatusMsg : public GbMsg {
    public:
        String myType() { return NAMEOF(GbGetStatusMsg); }
        GbGetStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {
        }

        GbGetStatusMsg(String nodeId) : GbMsg(NAMEOF(GbGetStatusMsg), nodeId) {
        }
        virtual ~GbGetStatusMsg() {};
};
