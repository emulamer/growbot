#include "GrowbotData.h"
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <WiFi.h>

#ifndef DATACONNECTION_H
#define DATACONNECTION_H


//protocol:
//commands come from MQTT, all binary like.
//offset        len         purpose
//0             4           COMMAND (uint32LE)

#define CMD_SET_OPERATING_MODE 0x2
#define CMD_SET_CONFIG 0x10

/*
    going to need some mode for calibration
*/
class DataConnection {
    public:
        virtual void init();
        virtual bool sendState(GrowbotState &state);
        virtual void onModeChange(std::function<void(uint8_t mode)> callback);
        virtual void onNewConfig(std::function<void(GrowbotConfig &newConfig)> callback);
};

 
class MQTTDataConnection : public DataConnection {
    private:
        AsyncMqttClient mqttClient;
        Ticker mqttReconnectTimer;
        const char* mqttPublishTopic;
        const char* mqttListenTopic;
        const char* mqttHost;
        short mqttPort;
        std::vector<std::function<void(uint8_t mode)>> onModeChangeCallbacks;
        std::vector<std::function<void(GrowbotConfig &newConfig)>> onNewConfigCallbacks;

        void connectMqtt() {
            Serial.println("connectMqtt..");
            if (!this->mqttClient.connected() && WiFi.isConnected()) {
                Serial.print("Connecting MQTT to");
                Serial.print(this->mqttHost);
                Serial.print(":");
                Serial.print(this->mqttPort);
                Serial.println("...");
                this->mqttClient.connect();
            } else {
                if (this->mqttClient.connected()) {
                    Serial.println("MQTT already connected");
                } else {
                    Serial.println("MQTT not in a state to connect, probably Wifi is not ready.");
                }
                
            }
        }
        void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
            this->connectMqtt();
        }
        void onMqttConnect(bool sessionPresent) {
            Serial.print("MQTT connected, subscribing to ");
            Serial.println(this->mqttListenTopic);
            this->mqttClient.subscribe(this->mqttListenTopic, 1);
        }
        void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
            Serial.println("Disconnected from MQTT.");

            if (WiFi.isConnected()) {
                this->mqttReconnectTimer.once(2, +[](MQTTDataConnection* instance) { instance->connectMqtt(); }, this);
            }
        }
        void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            Serial.println("Publish received.");
            Serial.print("  topic: ");
            Serial.println(topic);
            Serial.print("  qos: ");
            Serial.println(properties.qos);
            Serial.print("  dup: ");
            Serial.println(properties.dup);
            Serial.print("  retain: ");
            Serial.println(properties.retain);
            Serial.print("  len: ");
            Serial.println(len);
            Serial.print("  index: ");
            Serial.println(index);
            Serial.print("  total: ");
            Serial.println(total);
            if (strcmp(topic, this->mqttListenTopic) != 0) {
                Serial.print("got message on unknown topic ");
                Serial.println(topic);
                return;
            } 
            if (len < 1) {
                Serial.println("got an empty message");
                return;
            }
            switch ((uint8_t)payload[0]) {
                case CMD_SET_CONFIG:
                    if (sizeof(GrowbotConfig) < len - 1) {
                        Serial.print("Got set config message, but payload len of ");
                        Serial.print(len);
                        Serial.print(" is not the size of GrowbotConfig ");
                        Serial.println(sizeof(GrowbotConfig));
                        break;
                    }
                    Serial.println("got new config of the right size, doing callbacks");
                    for (auto callback : this->onNewConfigCallbacks) callback(*(GrowbotConfig*)(((uint8_t*)payload)+1));                    
                break;
                case CMD_SET_OPERATING_MODE:
                    if (len < 2) {
                        Serial.print("Got set operating mode message expecting 2 bytes total, but got ");
                        Serial.print(len);
                        break;
                    }
                    Serial.print("got new operating mode ");
                    Serial.print((uint8_t)payload[1]);
                    Serial.println(", doing callbacks");
                    for (auto callback : this->onModeChangeCallbacks) callback((uint8_t)payload[1]);
                break;
                default:
                    Serial.print("Unknown command received ");
                    Serial.println((uint8_t)payload[0]);
                    break;
            }           
        }
        void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
            Serial.println("Subscribe acknowledged.");
            Serial.print("  packetId: ");
            Serial.println(packetId);
            Serial.print("  qos: ");
            Serial.println(qos);
        }
    public:
        MQTTDataConnection(const char* host, const short port, const char* publishTopic, const char* listenTopic) {
            this->mqttPublishTopic = publishTopic;
            this->mqttListenTopic = listenTopic;
            this->mqttHost = host;
            this->mqttPort = port;
        }
        void init() {
            WiFi.onEvent(std::bind(&MQTTDataConnection::onWifiConnect, this,  std::placeholders::_1, std::placeholders::_2), SYSTEM_EVENT_STA_GOT_IP);
            this->mqttClient.onConnect(std::bind(&MQTTDataConnection::onMqttConnect, this,  std::placeholders::_1));
            this->mqttClient.onDisconnect(std::bind(&MQTTDataConnection::onMqttDisconnect, this,  std::placeholders::_1));//[this](AsyncMqttClientDisconnectReason reason) { this->onMqttDisconnect(reason); });
            this->mqttClient.onMessage(std::bind(&MQTTDataConnection::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
            this->mqttClient.onSubscribe(std::bind(&MQTTDataConnection::onMqttSubscribe, this, std::placeholders::_1, std::placeholders::_2));
            this->mqttClient.setServer(this->mqttHost, this->mqttPort);
            this->connectMqtt();
        }

        bool sendState(GrowbotState &state) {
            if (this->mqttClient.connected()) {
                this->mqttClient.publish(this->mqttPublishTopic, 1, false, (const char*)&state, sizeof(GrowbotState));
                return true;
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