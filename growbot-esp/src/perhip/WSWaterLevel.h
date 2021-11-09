#include <Arduino.h>
#include <DebugUtils.h>
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPRandom.h>
#include <GrowbotCommon.h>

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
        StaticJsonDocument<512> readDoc;
        bool connectWebsocket() {
            IPAddress newip;
            if (!Resolver.resolve(hostname, &newip)) {
                dbg.wprintln("WSWaterLevel: failed to resolve hostname!");
                return false;
            }
            if (newip != resolvedIP) {
                dbg.dprintf("WSWaterLevel: resolved ip has change from %s to %s\n", resolvedIP.toString(), newip.toString());
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
                    auto err = deserializeJson(readDoc, payload, length);
                    if (err) {
                        dbg.wprintf("WSWaterLevel: Failed to deserialize ws msg: %s\n", err.c_str());
                        return;
                    }
                    String event = readDoc["event"].as<String>();
                    if (event == NULL || event.length() < 1) {
                        dbg.wprintf("WSWaterLevel: message event was null or empty!");
                        return;
                    }
                    if (event.equals("status")) {
                        if (!readDoc.containsKey("waterLevel")) {
                            dbg.wprintln("WSWaterLevel: got a status update but it didn't have a water level");
                            return;
                        }
                        lastStatusLevel = readDoc["waterLevel"].as<float>();
                        lastStatusStamp = millis();
                    } else if (event.equals("cmdresult")) {
                        if (!readDoc["success"].as<bool>()) {
                            dbg.eprintf("WSWaterLevel: success was false or missing");
                            if (readDoc.containsKey("error")) {
                            dbg.eprintf(" error: %s\n", readDoc["error"].as<String>().c_str());
                            } else {
                                dbg.println("no error in response");
                            }
                            return;
                        }
                        if (!readDoc.containsKey("waterLevel")) {
                            dbg.wprintf("WSWaterLevel: response was missing waterLevel\n");
                            return;
                        }
                        replyWaterLevel = readDoc["waterLevel"].as<float>();
                        dbg.dprintf("WSWaterLevel: got response of %f\n", replyWaterLevel);
                    }
                    readDoc.clear();
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
                webSocket.onEvent(std::bind(&WSWaterLevel::webSocketEvent, this, _1, _2, _3));
                webSocket.setReconnectInterval(5000);
                
                isInitted = true;
            }
            return isConnected;
        }
    public:
        WSWaterLevel(const char* websocketHostname, int websocketPort) {
            this->hostname = websocketHostname;
            this->port = websocketPort;
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
            StaticJsonDocument<256> doc;
            uint8_t uuid_array[16];
            ESPRandom::uuid4(uuid_array);
            doc["msgid"] = ESPRandom::uuidToString(uuid_array);
            doc["cmd"] = "get";
            doc["field"] = "waterLevel";
            String jsonStr;
  
            if (serializeJson(doc, jsonStr) == 0) {
                reading.isComplete = true;
                reading.isSuccessful = false;
                reading.values[0] = NAN;
                return reading;
            }
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

