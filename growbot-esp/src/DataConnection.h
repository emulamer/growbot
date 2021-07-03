#include "GrowbotData.h"
#include <Arduino.h>
 #include <functional>
#ifndef DATACONNECTION_H
#define DATACONNECTION_H


//protocol:
//commands come from MQTT, all binary like.
//offset        len         purpose
//0             4           COMMAND (uint32LE)

#define CMD_SET_OPERATING_MODE 0x2
#define CMD_SET_CONFIG 0x10

class DataConnection {
    protected:
        std::vector<std::function<void(uint8_t mode)>> onModeChangeCallbacks;
        std::vector<std::function<void(GrowbotConfig &newConfig)>> onNewConfigCallbacks;
    public:
        virtual void init() = 0;
        virtual bool sendState(GrowbotState &state) = 0;
        void onModeChange(std::function<void(uint8_t mode)> callback) {
            this->onModeChangeCallbacks.push_back(callback);
        }
        void onNewConfig(std::function<void(GrowbotConfig &newConfig)> callback) {
            this->onNewConfigCallbacks.push_back(callback);
        }
};




#endif