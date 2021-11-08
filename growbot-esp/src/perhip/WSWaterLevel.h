#include <Arduino.h>
#include "../DebugUtils.h"
#include "../SensorBase.h"
#include "../GrowbotData.h"
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPRandom.h>

#pragma once
using namespace std::placeholders;

class WSWaterLevel : public SensorBase
{
    private:
        const char* hostname;
        int port;
        IPAddress resolvedIP = INADDR_NONE;
        WebSocketsClient webSocket;
        bool isConnected = false;
        bool isInitted = false;
        float replyWaterLevel = NAN;
        StaticJsonDocument<512> readDoc;
        bool checkResolve() {
            if (resolvedIP != INADDR_NONE) {
                return resolvedIP;
            }
            IPAddress serverIp = MDNS.queryHost(this->hostname);
            String ipStr = serverIp.toString();
            if (serverIp == (IPAddress)INADDR_NONE) {
                dbg.printf("WSWaterLevel: MDNS failed to resolve hostname!\n");
                return false;
            }
            resolvedIP = serverIp;
            return true;
        }
        void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
            switch(type) {
                case WStype_DISCONNECTED:
                    isConnected = false;
                    dbg.printf("[WSc] Disconnected!\n");
                    break;
                case WStype_CONNECTED:
                    isConnected = true;
                    dbg.printf("[WSc] Connected to url: %s\n", payload);
                    break;
                case WStype_TEXT:
                    dbg.printf("[WSc] get text: %s\n", payload);
                    auto err = deserializeJson(readDoc, payload, length);
                    if (err) {
                        dbg.printf("WSWaterLevel: Failed to deserialize ws msg: %s\n", err.c_str());
                        return;
                    }
                    if (!readDoc["success"].as<bool>()) {
                        dbg.printf("WSWaterLevel: success was false or missing");
                        if (readDoc.containsKey("error")) {
                          dbg.printf(" error: %s\n", readDoc["error"].as<String>().c_str());
                        } else {
                            dbg.println("no error in response");
                        }
                        return;
                    }
                    if (!readDoc.containsKey("waterLevel")) {
                        dbg.printf("WSWaterLevel: response was missing waterLevel\n");
                        return;
                    }
                    replyWaterLevel = readDoc["waterLevel"].as<float>();
                    dbg.printf("WSWaterLevel: got response of %f\n", replyWaterLevel);
                    readDoc.clear();
                    break;
                // case WStype_BIN:
                //     dbg.printf("[WSc] get binary length: %u\n", length);
                //     break;
                // case WStype_ERROR:			
                // case WStype_FRAGMENT_TEXT_START:
                // case WStype_FRAGMENT_BIN_START:
                // case WStype_FRAGMENT:
                // case WStype_FRAGMENT_FIN:
                // default:
                //     dbg.println("got some websocket stuff that isn't handled");
                //     break;
            }

        }
        bool checkConnect() {
            if (!WiFi.isConnected()) {
                dbg.println("WSWaterLevel: wifi isn't connected yet...");
                return false;
            }
            if (!isInitted) {
                if (!checkResolve()) {
                    dbg.println("WSWaterLevel: failed to resolve hostname");
                    return false;
                }
                webSocket.begin(resolvedIP.toString(), port, "/");
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
            if (!this->checkConnect()) {
                dbg.println("WSWaterLevel: not connected!");
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
           if (replyWaterLevel == NAN) {
               dbg.println("WSWaterLevel: apparently didn't get a reply with water level");
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

