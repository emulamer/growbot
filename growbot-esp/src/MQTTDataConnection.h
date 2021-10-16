#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>
#include "DebugUtils.h"
#include "GrowbotData.h"
#include "DataConnection.h"
#include <mdns.h>

#ifndef MQTTDATACONNECTION_H
#define MQTTDATACONNECTION_H
class MQTTDataConnection : public DataConnection {
    private:
        AsyncMqttClient mqttClient;
        //Ticker mqttReconnectTimer;
        const char* mqttPublishTopic;
        const char* mqttListenTopic;
        const char* mqttHost;
        short mqttPort;
        unsigned long reconnectAt = -1;
        void reconnectTimerTick() {
            reconnectTimerSet = false;
            if (!WiFi.isConnected()) {
                dbg.printf("MQTT: Wifi isn't connected, will try rescheduling reconnect again.\n");
                startReconnectTimer();
            } else {
                dbg.printf("MQTT: Wifi is connected, doing mqtt connect.\n");
                connectMqtt();
            }            
        }

        void connectMqtt() {
            dbg.println("connectMqtt..");
            if (mqttClient.connected()) {
                dbg.println("connectMqtt called but MQTT is already conected.");
                return;
            }
            if (!WiFi.isConnected()) {
                dbg.println("MQTT not in a state to connect, probably Wifi is not ready.");
            } else {
                dbg.printf("MQTT: mDNS resolving %s.local\n", mqttHost);
                bool mdnsResolved = false;
                struct ip4_addr addr;
                addr.addr = 0;
                esp_err_t err = mdns_query_a(mqttHost, 2000,  &addr);
                if(err){
                    if(err == ESP_ERR_NOT_FOUND){
                        dbg.printf("mDNS Host %s was not found!\n", mqttHost);
                        mdnsResolved = false;
                    }
                    dbg.printf("mDNS Query Failed\n");
                    mdnsResolved = false;
                } else {
                    mdnsResolved = true;
                    dbg.printf("mDNS Resolved %s to %d.%d.%d.%d\n", mqttHost, IP2STR(&addr));
                }
                
                if (mdnsResolved) {
                    dbg.printf("MQTT connecting to resovled mDNS IP %d.%d.%d.%d port %d\n", IP2STR(&addr), this->mqttPort);
                    IPAddress mdnsAddress = IPAddress(addr.addr);
                    this->mqttClient.setServer(mdnsAddress, this->mqttPort);
                } else {
                    dbg.printf("MQTT connecting with hostname %s port %d\n", this->mqttHost, this->mqttPort);
                    this->mqttClient.setServer(this->mqttHost, this->mqttPort);
                }


                
                dbg.printf("Connecting MQTT to %s:%d...\n", this->mqttHost, this->mqttPort);
                this->mqttClient.connect();
            
            }
        }
        void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
            this->connectMqtt();
        }
        void onMqttConnect(bool sessionPresent) {
            dbg.printf("MQTT connected, subscribing to %s\n", this->mqttListenTopic);
            this->mqttClient.subscribe(this->mqttListenTopic, 1);
        }
        void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
            dbg.println("Disconnected from MQTT.");

            startReconnectTimer();
        }
        bool reconnectTimerSet = false;
        void startReconnectTimer() {
            if (!reconnectTimerSet) {
                dbg.printf("Starting reconnect timer...\n");
                reconnectTimerSet = true;
                reconnectAt = millis() + 2000;
                //this->mqttReconnectTimer.once(2, +[](MQTTDataConnection* instance) { instance->reconnectTimerTick(); }, this);
            } else {
                dbg.printf("Reconnect timer already set.\n");
            }
        }
        void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            
            if (strcmp(topic, this->mqttListenTopic) != 0) {
                dbg.print("got message on unknown topic ");
                dbg.println(topic);
                return;
            } 
            if (len < 1) {
                dbg.println("got an empty message");
                return;
            }
            switch ((uint8_t)payload[0]) {
                case CMD_SET_CONFIG:
                    if (sizeof(GrowbotConfig) < len - 1) {
                        dbg.printf("Got set config message, but payload len of %d is not the size of GrowboxConfig %d\n", len, sizeof(GrowbotConfig));
                        break;
                    }
                    dbg.println("got new config of the right size, doing callbacks");
                    for (auto callback : this->onNewConfigCallbacks) callback(*(GrowbotConfig*)(((uint8_t*)payload)+1));                    
                break;
                case CMD_SET_OPERATING_MODE:
                    if (len < 2) {
                        dbg.print("Got set operating mode message expecting 2 bytes total, but got ");
                        dbg.print(len);
                        break;
                    }
                    dbg.print("got new operating mode ");
                    dbg.print((uint8_t)payload[1]);
                    dbg.println(", doing callbacks");
                    for (auto callback : this->onModeChangeCallbacks) callback((uint8_t)payload[1]);
                break;
                default:
                    dbg.printf("Unknown command received %d\n", (uint8_t)payload[0]);
                    break;
            }           
        }
        void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
            dbg.printf("MQTT subscribe acknowledged\n");
        }
    public:
        MQTTDataConnection(const char* host, const short port, const char* publishTopic, const char* listenTopic) {
            this->mqttPublishTopic = publishTopic;
            this->mqttListenTopic = listenTopic;
            this->mqttHost = host;
            this->mqttPort = port;
        }
        void handle() {
            if (this->reconnectTimerSet && this->reconnectAt <= millis()) {
                this->reconnectTimerTick();
                this->reconnectTimerSet = false;
            }

        }
        void init() {
            WiFi.onEvent(std::bind(&MQTTDataConnection::onWifiConnect, this,  std::placeholders::_1, std::placeholders::_2), SYSTEM_EVENT_STA_GOT_IP);
            this->mqttClient.onConnect(std::bind(&MQTTDataConnection::onMqttConnect, this,  std::placeholders::_1));
            this->mqttClient.onDisconnect(std::bind(&MQTTDataConnection::onMqttDisconnect, this,  std::placeholders::_1));//[this](AsyncMqttClientDisconnectReason reason) { this->onMqttDisconnect(reason); });
            this->mqttClient.onMessage(std::bind(&MQTTDataConnection::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            this->mqttClient.onSubscribe(std::bind(&MQTTDataConnection::onMqttSubscribe, this, std::placeholders::_1, std::placeholders::_2));
            this->connectMqtt();
        }

        bool sendState(GrowbotState &state) {
            if (this->mqttClient.connected()) {
                this->mqttClient.publish(this->mqttPublishTopic, 1, false, (const char*)&state, sizeof(GrowbotState));
                return true;
            } else {
                dbg.printf("MQTT couldn't sent state, not connected!:\n");
                startReconnectTimer();
            }
            return false;
        }

        void onModeChange(std::function<void(uint8_t mode)> callback) {
            this->onModeChangeCallbacks.push_back(callback);
        }
        void onNewConfig(std::function<void(GrowbotConfig &newConfig)> callback) {
            this->onNewConfigCallbacks.push_back(callback);
        }

};

#endif