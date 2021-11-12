#include <Arduino.h>
#include <DebugUtils.h>
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <EzEsp.h>
#include <MessageParser.h>

#pragma once
using namespace std::placeholders;

#define STATUS_MAX_AGE_MS 10000

class WSWaterLevel : public SensorBase
{
    private:
        const char* hostname;
        int port;
        IPAddress resolvedIP = INADDR_ANY;
        WebSocketsClient webSocket;
        bool isConnected = false;
        bool isInitted = false;
        float replyWaterLevel = NAN;

        float lastStatusLevel = NAN;
        unsigned long lastStatusStamp = 0;
        //StaticJsonDocument<512> readDoc;
        bool connectWebsocket() {
            IPAddress newip;
            if (!Resolver.resolve(hostname, &newip)) {
                dbg.wprintln("WSWaterLevel: failed to resolve hostname!");
                return false;
            }
            if (newip != resolvedIP) {
                dbg.dprintf("WSWaterLevel: resolved ip has change from %s to %s\n", resolvedIP.toString().c_str(), newip.toString().c_str());
                resolvedIP = newip;
            }
            webSocket.begin(resolvedIP, port, "/");
            return true;
        }
        void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
            switch(type) {
                case WStype_DISCONNECTED:
                    isConnected = false;
                    dbg.eprintf("WSWaterLevel: websocket disconnected!\n");
                    connectWebsocket();
                    break;
                case WStype_CONNECTED:
                    isConnected = true;
                    dbg.println("WSWaterLevel: websocket connected");
                    break;
                case WStype_TEXT:
                    GbMsg* msg = parseGbMsg((char*)payload, length);
                    if (msg == NULL) {
                        dbg.wprintln("WSWaterLevel: unparseable message!");
                        return;
                    }
                    if (msg->myType().equals(NAMEOF(FlowStatusGbMsg))) {
                        FlowStatusGbMsg* m = (FlowStatusGbMsg*)msg;
                        float lvl = m->waterLevel();
                        if (!isnan(lvl)) {
                            lastStatusLevel = lvl;
                            lastStatusStamp = millis();
                        } else {
                            dbg.println("WSWaterLevel: got nan water level!");
                        }
                    } else {
                        dbg.dprintf("WsWaterLevel: some other msg type we don't care: %s\n", msg->myType().c_str());
                    }
                    delete msg;
                    break;
            }

        }
        bool checkConnect() {
            if (!WiFi.isConnected()) {
                dbg.wprintln("WSWaterLevel: wifi isn't connected yet...");
                return false;
            }
            if (!isInitted) {
                if (!connectWebsocket()) {
                    return false;
                }                
                isInitted = true;
            }
            return isConnected;
        }
    public:
        WSWaterLevel(const char* websocketHostname, int websocketPort) {
            this->hostname = websocketHostname;
            this->port = websocketPort;
            webSocket.onEvent(std::bind(&WSWaterLevel::webSocketEvent, this, _1, _2, _3));
            webSocket.setReconnectInterval(5000);
        }       

        DeferredReading startRead() {
            replyWaterLevel = NAN;
            DeferredReading reading;
            reading.isComplete = false;
            reading.readingCount = 1;
            reading.deferUntil = 0;

            if (!isnan(lastStatusLevel) && millis() - lastStatusStamp <= STATUS_MAX_AGE_MS) {
                //already have a recent water level from a status update
                reading.isComplete = true;
                reading.isSuccessful = true;
                reading.values[0] = lastStatusLevel;
                return reading;
            }
            if (!this->checkConnect()) {
                dbg.eprintln("WSWaterLevel: not connected!");
                reading.isComplete = true;
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            }
            GbGetStatusMsg msg(WiFi.macAddress());
            String jsonStr = msg.toJson();  
            webSocket.sendTXT(jsonStr);
            reading.deferUntil = millis() + 750; //should be able to get a response within 750ms I hope?
            return reading;
        }
        void finishRead(DeferredReading &reading) {
            if (reading.isComplete) {
                return;
            }
            if (replyWaterLevel == NAN) {
                dbg.wprintln("WSWaterLevel: apparently didn't get a reply with water level");
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return;
            }
            reading.isSuccessful = true;
            reading.values[0] = replyWaterLevel;
            replyWaterLevel = NAN;
        }

        void handle() {
            this->webSocket.loop();
        }

        void init() {
            checkConnect();
        }
};

