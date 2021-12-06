#include <Arduino.h>
#include <Resolver.h>
#include <DebugUtils.h>
#include <StaticServer.h>

#ifndef SWITCHEROO_H
#define SWITCHEROO_H

#define HTTP_TIMEOUT 3000
#define RETRY_INTERVAL_MS 15000
class SwitcherooWiFi {
    private: 
        IPAddress resolvedIP = INADDR_ANY;
        unsigned long lastIncomingStamp = 0;
        int lastIncomingFlags = -1;

        int powerFlags = -1;

        unsigned long lastRetryStamp = 0;
        byte parsePowerFlags(SwitcherooStatusMsg* ms) {
            byte status = 0;
            for (int i = 0; i < 6; i++) {
                if (ms->ports().portStatus[i]) {
                    status |= 1<<i;
                }
            }
            return status;
        }

        void onSwitcherooStatus(MessageWrapper& mw) {
            dbg.println("got switcheroo status msg");
            SwitcherooStatusMsg* msg = (SwitcherooStatusMsg*)mw.message;
            byte newStatus = parsePowerFlags(msg);
            lastIncomingFlags = newStatus;
            lastIncomingStamp = millis();
            if (powerFlags < 0) {
                dbg.dprintln("SwitcherooWiFi: power flags wasn't set, so updated it from status");
                powerFlags = lastIncomingFlags;
            }
        }
        bool sendFlags() {
            if (powerFlags < 0) {
                dbg.eprintf("SwitcherooWiFi: power flags is invalid: %d\n", powerFlags);
                return false;
            }
            dbg.dprintf("Switcheroo: setting power flags: %d\n", this->powerFlags);
            SwitcherooSetPortsMsg msg;
            for (int i = 0; i < 6; i++) {
                msg.setPort(i, (powerFlags & (1<<i)) > 0);
            }
            return server.broadcast(msg);            
        }
    public:
        SwitcherooWiFi() {
            server.onMessage(NAMEOF(SwitcherooStatusMsg), std::bind(&SwitcherooWiFi::onSwitcherooStatus, this, std::placeholders::_1));
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