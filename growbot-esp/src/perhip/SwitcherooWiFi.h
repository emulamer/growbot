#include <Arduino.h>
#include <Resolver.h>
#include <DebugUtils.h>

#ifndef SWITCHEROO_H
#define SWITCHEROO_H

#define HTTP_TIMEOUT 3000
#define RETRY_INTERVAL_MS 15000
class SwitcherooWiFi {
    private: 
        const char* hostname;
        int port = 8118;
        IPAddress resolvedIP = INADDR_ANY;
        WebSocketsClient webSocket;
        bool isConnected = false;
        bool isInitted = false;
        unsigned long lastIncomingStamp = 0;
        int lastIncomingFlags = -1;

        int powerFlags = -1;

        unsigned long lastRetryStamp = 0;
        StaticJsonDocument<512> readDoc;
        bool connectWebsocket() {
            IPAddress newip;
            if (!Resolver.resolve(hostname, &newip)) {
                dbg.wprintln("SwitcherooWiFi: failed to resolve hostname!");
                return false;
            }
            if (newip != resolvedIP) {
                dbg.dprintf("SwitcherooWiFi: resolved ip has change from %s to %s\n", resolvedIP.toString().c_str(), newip.toString().c_str());
                resolvedIP = newip;
            }
            dbg.dprintf("SwitcherooWiFi: connecting websocket to %s on port %d\n", resolvedIP.toString().c_str(), port);
            webSocket.begin(resolvedIP, port, "/");
            return true;
        }
        int parsePowerFlags(StaticJsonDocument<512> &doc) {
            if (!doc.containsKey("ports")) {
                dbg.eprintln("SwitcherooWiFi: can't parse, no ports");
                return -1;
            }
            byte status = 0;
            bool hasOne = false;
            if (doc["ports"].containsKey("port0") && doc["ports"]["port0"].as<bool>()) {
                status |= 1;
                hasOne = true;
            }
            if (doc["ports"].containsKey("port1") && doc["ports"]["port1"].as<bool>()) {
                status |= 1<<1;
                hasOne = true;
            }
            if (doc["ports"].containsKey("port2") && doc["ports"]["port2"].as<bool>()) {
                status |= 1<<2;
                hasOne = true;
            }
            if (doc["ports"].containsKey("port3") && doc["ports"]["port3"].as<bool>()) {
                status |= 1<<3;
                hasOne = true;
            }
            if (doc["ports"].containsKey("port4") && doc["ports"]["port4"].as<bool>()) {
                status |= 1<<4;
                hasOne = true;
            }
            if (doc["ports"].containsKey("port5") && doc["ports"]["port5"].as<bool>()) {
                status |= 1<<5;
                hasOne = true;
            }
            if (!hasOne) {
                dbg.eprintln("SwitcherooWiFi: can't parse, doc ports had no ports");
                return -1;
            }
            return status;
        }
        void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
            switch(type) {
                case WStype_DISCONNECTED:
                    isConnected = false;
                    dbg.eprintf("SwitcherooWiFi: websocket disconnected!\n");
                    connectWebsocket();
                    break;
                case WStype_CONNECTED:
                    isConnected = true;
                    dbg.println("SwitcherooWiFi: websocket connected");
                    if (this->powerFlags >= 0) {
                        dbg.wprintln("Switcheroo: current flags are set, sending them");
                        sendFlags();
                    }
                    break;
                case WStype_TEXT:
                    dbg.dprintf("switcheroo msg: %s\n", payload);
                    auto err = deserializeJson(readDoc, payload, length);
                    if (err) {
                        dbg.wprintf("SwitcherooWiFi: Failed to deserialize ws msg: %s\n", err.c_str());
                        return;
                    }
                    String event = readDoc["event"].as<String>();
                    if (event == NULL || event.length() < 1) {
                        dbg.wprintf("SwitcherooWiFi: message event was null or empty!\n");
                        return;
                    }
                    
                    if (event.equals("status")) {
                        if (!readDoc.containsKey("ports")) {
                            dbg.wprintln("SwitcherooWiFi: got a status update but it didn't have ports");
                            return;
                        }
                        int newStatus = parsePowerFlags(readDoc);
                        if (newStatus < 0) {
                            dbg.eprintf("status had invalid message!\n");
                        } else {
                            lastIncomingFlags = newStatus;
                            lastIncomingStamp = millis();
                            if (powerFlags < 0) {
                                dbg.dprintln("SwitcherooWiFi: power flags wasn't set, so updated it from status");
                                powerFlags = lastIncomingFlags;
                            }
                        }
                    } else if (event.equals("cmdresult")) {
                        if (!readDoc["success"].as<bool>()) {
                            dbg.eprintf("SwitcherooWiFi: success was false or missing\n");
                            if (readDoc.containsKey("error")) {
                            dbg.eprintf(" error: %s\n", readDoc["error"].as<String>().c_str());
                            } else {
                                dbg.println("no error in response");
                            }
                            return;
                        }
                        if (readDoc.containsKey("cmd") && strcmp(readDoc["cmd"].as<String>().c_str(), "get") == 0) {
                            int replyFlags = parsePowerFlags(readDoc);
                            if (replyFlags < 0) {
                                dbg.eprintln("SwitcherooWiFi: command response flags were invalid");
                            } else {
                                dbg.dprintf("SwitcherooWiFi: got command response of flags: %d\n", replyFlags);
                                lastIncomingFlags = replyFlags;
                                lastIncomingStamp = millis();
                                if (powerFlags < 0) {
                                    dbg.dprintln("SwitcherooWiFi: power flags wasn't set, so updated it");
                                    powerFlags = replyFlags;
                                }
                            }
                        }
                    }
                    readDoc.clear();
                    break;
            }

        }
        bool checkConnect() {
            if (!WiFi.isConnected()) {
                dbg.wprintln("SwitcherooWiFi: wifi isn't connected yet...");
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
        void requestPowerFlags() {
            if (!this->checkConnect()) {
                dbg.eprintln("SwitcherooWiFi: can't get powerflags because it isn't connected yet");
                return;
            }
            StaticJsonDocument<512> doc;
            doc["cmd"] = "get";
            String jsonStr;  
            if (serializeJson(doc, jsonStr) == 0) {
                dbg.eprintln("Failed to serialize get json");
            } else {
                webSocket.sendTXT(jsonStr);
            }
        }
        bool sendFlags() {
            if (powerFlags < 0) {
                dbg.eprintf("SwitcherooWiFi: power flags is invalid: %d\n", powerFlags);
                return false;
            }
            if (!this->checkConnect()) {
                dbg.eprintln("Switcheroo: can't send power flags, websocket isn't connected yet!");
                return false;
            }
            dbg.dprintf("Switcheroo: setting power flags: %d\n", this->powerFlags);
            
            StaticJsonDocument<512> doc;
            doc["cmd"] = "setports";
            doc["ports"]["port0"] = ((powerFlags & 1) > 0);
            doc["ports"]["port1"] = ((powerFlags & (1<<1)) > 0);
            doc["ports"]["port2"] = ((powerFlags & (1<<2)) > 0);
            doc["ports"]["port3"] = ((powerFlags & (1<<3)) > 0);
            doc["ports"]["port4"] = ((powerFlags & (1<<4)) > 0);
            doc["ports"]["port5"] = ((powerFlags & (1<<5)) > 0);
        
            String jsonStr;
  
            if (serializeJson(doc, jsonStr) == 0) {
                dbg.eprintln("Failed to serialize reply json");
                return false;
            } else {
                return webSocket.sendTXT(jsonStr);
            }
        }
    public:
        SwitcherooWiFi(const char* hostname) {
            this->hostname = hostname;
            webSocket.onEvent(std::bind(&SwitcherooWiFi::webSocketEvent, this, _1, _2, _3));
            webSocket.setReconnectInterval(5000);
        }
        void init() {
        }
        bool getPowerToggle(byte portNum) {
            if (powerFlags < 0) {
                return false;
            }
            byte flag = 1<<portNum;
            return (flag & this->powerFlags) != 0;
        }
        void handle() {
            webSocket.loop();
            if (this->powerFlags >= 0 && this->powerFlags != this->lastIncomingFlags && (millis() - lastRetryStamp) > RETRY_INTERVAL_MS) {
                dbg.wprintln("Switcheroo: last sent flags don't match current flags, retrying...");
                sendFlags();
                lastRetryStamp = millis();
            }
        }
        bool setPowerToggle(byte portNum, bool on) {
            if (powerFlags < 0) {
                powerFlags = 0;
            }
            byte flag = 1<<portNum;
            if (on) {
                this->powerFlags |= flag;
            } else {
                this->powerFlags &= ~flag;
            }
            if (this->powerFlags == this->lastIncomingFlags) {
                dbg.dprintf("Switcheroo: power flags being set match last good sent flags, skipping\n");    
                return true;
            }

            return sendFlags();            
        }
};

#endif