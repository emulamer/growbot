#include <Arduino.h>
#include <GbMsg.h>

#pragma once

class TempSensorStatusMsg : public GbMsg {
    public:
        String myType() {return NAMEOF(TempSensorStatusMsg);}
        TempSensorStatusMsg(StaticJsonDocument<MSG_JSON_SIZE> ref) : GbMsg(ref) {};
        TempSensorStatusMsg(String nodeId, float tempC, float humidityPercent) : GbMsg(__FUNCTION__, nodeId) { 
            if (isnan(tempC)) {
               // (*this)["tempC"] = NULL;
            } else {
                (*this)["tempC"] = tempC;
            }

            if (isnan(humidityPercent)) {
              //  (*this)["humidityPercent"] = NULL;
            } else {
                (*this)["humidityPercent"] = humidityPercent;
            }
            
            
        }

        float tempC() {
            if ((*this)["tempC"].isNull()) {
                return NAN;
            }
            return (*this)["tempC"].as<float>();
        }
        float humidityPercent() {
            if ((*this)["humidityPercent"].isNull()) {
                return NAN;
            }
            return (*this)["humidityPercent"].as<float>();
        }
        virtual ~TempSensorStatusMsg() {}
};