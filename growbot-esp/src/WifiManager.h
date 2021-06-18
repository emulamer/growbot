#include <WiFi.h>
#include <Ticker.h>

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H
class WifiManager {
    private:
        const char* ssid;
        const char* password;
        Ticker wifiReconnectTimer;
        IPAddress emptyip = IPAddress(0,0,0,0);
        void connectWifi() {
            if (!WiFi.isConnected() || WiFi.localIP() == emptyip) {
                WiFi.mode(WIFI_STA);
                WiFi.begin(this->ssid, this->password);
            } else {
                Serial.println("Wifi tried reconnecting while already connected, ignoring...");
            }
        }

        void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.println("Wifi connected, waiting for IP...");
        }
        void onWifiGotIP(WiFiEvent_t event) {
            Serial.print("Wifi got IP ");
            Serial.println(WiFi.localIP().toString());  
        }

        void onWifiDisconnect(WiFiEvent_t event) {
            Serial.println("Wifi disconnected");
            wifiReconnectTimer.once(6, +[](WifiManager* instance) { instance->connectWifi(); }, this);
        }

    public: 
        WifiManager(const char* defaultSSID, const char* defaultPassword) {
            this->ssid = defaultSSID;
            this->password = defaultPassword;
            WiFi.onEvent(std::bind(&WifiManager::onWifiGotIP, this, std::placeholders::_1), SYSTEM_EVENT_STA_GOT_IP);  
            WiFi.onEvent(std::bind(&WifiManager::onWifiDisconnect, this, std::placeholders::_1), SYSTEM_EVENT_STA_DISCONNECTED);
            WiFi.onEvent(std::bind(&WifiManager::onWifiConnect, this, std::placeholders::_1, std::placeholders::_2), SYSTEM_EVENT_STA_CONNECTED);
        }
        void init() {
            connectWifi();
        }
};
#endif