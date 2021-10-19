#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include "GrowbotData.h"
#include "perhip/SwitcherooWiFi.h"
#include "DebugUtils.h"

#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#define TIMEKEEPER_TICK_INTERVAL 30000

class TimeKeeper {
    private:
        static bool initted;
        static WiFiUDP ntpUdp;
        static NTPClient* timeClient;
        bool* onFlag;
        byte portNum;
        SwitcherooWiFi* switcher;
        SwitchSchedule* schedule;
        unsigned long nextTick = 0;
        bool tickNow() {
            unsigned long now = millis();
            if (nextTick <= now) {
                nextTick = millis() + TIMEKEEPER_TICK_INTERVAL;
                return true;
            }
            return false;
        }
    public:
        TimeKeeper(SwitcherooWiFi* switcher, byte portNum, SwitchSchedule* schedule, bool* onFlag) {
            this->switcher = switcher;
            this->portNum = portNum;
            this->schedule = schedule;
            this->onFlag = onFlag;
        }

        //returns true if it changed something
        bool handle(bool forceNow = false) {
            if (!this->tickNow() && !forceNow) {
                return false;
            }
            if (!TimeKeeper::initted) {
                if (WiFi.status() == WL_CONNECTED) {
                    dbg.printf("TimeKeeper: Initting now that Wifi is available\n");
                    TimeKeeper::timeClient = new NTPClient(TimeKeeper::ntpUdp, "pool.ntp.org");
                    TimeKeeper::timeClient->begin();
                    TimeKeeper::initted = true;
                } else {
                    dbg.printf("TimeKeeper isn't initted yet because Wifi isn't available yet\n");
                    return false;
                }
            }
            TimeKeeper::timeClient->update();
            int timeNow = (this->timeClient->getHours() * 100) + this->timeClient->getMinutes();    
            dbg.printf("TimeKeeper: tick at time %d\n", timeNow);
            bool on = false;
            if (this->schedule->status == 0) {
                if (this->schedule->turnOffGmtHourMin < this->schedule->turnOnGmtHourMin) {
                    if (timeNow >= this->schedule->turnOffGmtHourMin && timeNow < this->schedule->turnOnGmtHourMin) {
                        on = false;
                    } else {
                        on = true;
                    }
                } else {
                    if (timeNow >= this->schedule->turnOnGmtHourMin && timeNow < this->schedule->turnOffGmtHourMin) {
                        on = true;
                    } else {
                        on = false;
                    }
                }
                
                this->switcher->setPowerToggle(this->portNum, on);
            } else {
                on = (this->schedule->status==1)?true:false;
            } 
            bool ret = false;
            
            
            if (this->switcher->getPowerToggle(this->portNum) != on) {
                dbg.printf("TimeKeeper: changing port %d to state %s\n", this->portNum, on?"ON":"OFF");
                ret = true;
            }
            this->switcher->setPowerToggle(this->portNum, on);
            *this->onFlag = on;
            return ret;
        }
};

bool TimeKeeper::initted;
WiFiUDP TimeKeeper::ntpUdp;
NTPClient* TimeKeeper::timeClient;

#endif