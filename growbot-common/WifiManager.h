#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "DebugUtils.h"
#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#define WIFI_STRENGTH_REPORT_FREQ_MS 5000
class WifiManager {
    private:
        const char* ssid;
        const char* password;
        const char* hostname;
        unsigned long lastReportStamp = 0;
        IPAddress emptyip = IPAddress(0,0,0,0);
        unsigned long nextReconnectStamp = 0;
        void connectWifi() {
            if (!WiFi.isConnected() || WiFi.localIP() == emptyip) {
                WiFi.mode(WIFI_STA);
                WiFi.begin(this->ssid, this->password);
            } else {
                dbg.println("Wifi tried reconnecting while already connected, ignoring...");
            }
        }

        void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
            dbg.println("Wifi connected, waiting for IP...");
        }
        void onWifiGotIP(WiFiEvent_t event) {
            dbg.printf("Wifi got IP %s\n",WiFi.localIP().toString().c_str());
            dbg.wifiIsReady();
            dbg.println("Set wifi ready for debug");
        }

        void onWifiDisconnect(WiFiEvent_t event) {
            dbg.println("Wifi disconnected");
            nextReconnectStamp = millis() + 6000;
        }

    public: 
        WifiManager() {
            WiFi.onEvent(std::bind(&WifiManager::onWifiGotIP, this, std::placeholders::_1), SYSTEM_EVENT_STA_GOT_IP);  
            WiFi.onEvent(std::bind(&WifiManager::onWifiDisconnect, this, std::placeholders::_1), SYSTEM_EVENT_STA_DISCONNECTED);
            WiFi.onEvent(std::bind(&WifiManager::onWifiConnect, this, std::placeholders::_1, std::placeholders::_2), SYSTEM_EVENT_STA_CONNECTED);
        }
        void init(const char* hostname, const char* defaultSSID, const char* defaultPassword) {
            this->ssid = defaultSSID;
            this->password = defaultPassword;
            this->hostname = hostname;
            WiFi.hostname(hostname);
            connectWifi();
        }
        void handle() {
            if (nextReconnectStamp > 0 && millis() - nextReconnectStamp > 0) {
                nextReconnectStamp = 0;
                connectWifi();
            }
            if (lastReportStamp < millis()) {
                if (!WiFi.isConnected()) {
                    dbg.printf("Wifi SSID %s not connected!\n", this->ssid)    ;
                } else {
                    dbg.printf("Wifi SSID %s connected, strength %d\n", this->ssid, WiFi.RSSI());
                }   
                lastReportStamp = millis() + WIFI_STRENGTH_REPORT_FREQ_MS;             
            }
        }
};
#endif