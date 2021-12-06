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
#include <StaticServer.h>

#pragma once
using namespace std::placeholders;

#define STATUS_MAX_AGE_MS 10000

class WSWaterLevel : public SensorBase
{
    private:
        const char* hostname;
        int port;
        //IPAddress resolvedIP = INADDR_ANY;
        //WebSocketsClient webSocket;
        //bool isConnected = false;
        bool isInitted = false;
        float replyWaterLevel = NAN;

        float lastStatusLevel = NAN;
        unsigned long lastStatusStamp = 0;
        //StaticJsonDocument<512> readDoc;
        // bool connectWebsocket() {
        //     IPAddress newip;
        //     if (!Resolver.resolve(hostname, &newip)) {
        //         dbg.wprintln("WSWaterLevel: failed to resolve hostname!");
        //         return false;
        //     }
        //     if (newip != resolvedIP) {
        //         dbg.dprintf("WSWaterLevel: resolved ip has change from %s to %s\n", resolvedIP.toString().c_str(), newip.toString().c_str());
        //         resolvedIP = newip;
        //     }
        //     webSocket.begin(resolvedIP, port, "/");
        //     return true;
        // }
        void onFlowMsg(MessageWrapper& mw) {
            FlowStatusMsg* m = (FlowStatusMsg*)mw.message;
            float lvl = m->waterLevel();
            if (!isnan(lvl)) {
                lastStatusLevel = lvl;
                lastStatusStamp = millis();
            } else {
                dbg.println("WSWaterLevel: got nan water level!");
            }
        }
    public:
        WSWaterLevel(const char* websocketHostname, int websocketPort) {
            this->hostname = websocketHostname;
            this->port = websocketPort;
            server.onMessage(NAMEOF(FlowStatusMsg), std::bind(&WSWaterLevel::onFlowMsg, this, std::placeholders::_1));
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
            } else {
                reading.isComplete = true;
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            }
        }
        void finishRead(DeferredReading &reading) {
        }

        void handle() {
        }

        void init() {
        }
};

