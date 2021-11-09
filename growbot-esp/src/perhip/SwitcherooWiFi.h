#include <Arduino.h>
#include <Wire.h>
#include "I2CMultiplexer.h"
#include <HTTPClient.h>
#include <Resolver.h>
#include <DebugUtils.h>

#ifndef SWITCHEROO_H
#define SWITCHEROO_H

#define HTTP_TIMEOUT 3000
#define RETRY_INTERVAL_MS 15000
class SwitcherooWiFi {
    private: 
        String hostname;
        String* baseUrl = NULL;
        IPAddress ipAddr;
        byte powerFlags = 0;
        HTTPClient http;
        byte lastGoodSendFlags = -1;
        unsigned long lastRetryStamp = 0;
        bool checkBaseUrl() {
            IPAddress newIp;
            if (!Resolver.resolve(hostname, &newIp)) {
                dbg.eprintln("Switcheroo: unable to resolve hostname!");
                return false;
            }
            if (newIp != ipAddr) {
                ipAddr = newIp;
                if (baseUrl != NULL) {
                    delete baseUrl;
                    baseUrl = NULL;
                }
                this->baseUrl = new String(String("http://") + ipAddr.toString() + String("/"));
            }
            dbg.dprintf("Switcheroo: base url set to %s\n", this->baseUrl->c_str());
            return true;
        }
        bool getPowerFlags() {
            if (!this->checkBaseUrl()) {
                return false;
            }
            dbg.dprintf("Switcheroo: HTTP client making request to url %s\n", this->baseUrl->c_str());
            if (!this->http.begin(this->baseUrl->c_str())) {
                dbg.eprintf("Switcheroo: HTTP client begin returned false!\n");
                return false;
            }
            bool ok = false;
            int code = this->http.GET();
            if (code != 200) {
                dbg.eprintf("Switcheroo: HTTP client GET failed with status %d!\n", code);
            } else {
                String resp = http.getString();
                if (resp.length() < 1 || resp.length() > 3) {
                    dbg.eprintf("Switcheroo: HTTP client GET response was wrong length, resp: %s\n", resp.c_str());    
                } else {
                    bool bad = false;
                    for (int i = 0; i < resp.length(); i++) {
                        if (!isDigit(resp[i])) {
                            bad = true;
                            break;
                        }
                    }
                    if (bad) {
                        dbg.eprintf("Switcheroo: HTTP client GET response was wrong not numeric, resp: %s\n", resp.c_str());
                        ok = false;
                    } else {
                        int portStatusNum = resp.toInt();
                        dbg.eprintf("Switcheroo: HTTP client got port status of %d\n", portStatusNum);
                        if (portStatusNum < 0 || portStatusNum > 255) {
                            dbg.eprintf("Switcheroo: HTTP client got out of range number %d\n", portStatusNum);
                        } else {
                            this->powerFlags = (byte)portStatusNum;
                            this->lastGoodSendFlags = this->powerFlags;
                            ok = true;
                        }
                    }
                }                
            }
            this->http.end();
            return ok;
        }
        bool sendFlags() {
            if (!this->checkBaseUrl()) {
                dbg.eprintln("Switcheroo: can't send power flags, name hasn't been resolved yet!");
                return false;
            }
            dbg.dprintf("Switcheroo: setting power flags: %d\n", this->powerFlags);
            String url = *this->baseUrl + String(this->powerFlags);
            dbg.dprintf("Switcheroo: HTTP client making request to url %s\n", url.c_str());
             if (!this->http.begin(url.c_str())) {
                dbg.eprintf("Switcheroo: HTTP client begin returned false!\n");
                return false;
            }
            bool ok = false;
            int code = this->http.GET();
            if (code != 200) {
                dbg.eprintf("Switcheroo: HTTP client GET failed with status %d!\n", code);
            } else {
                dbg.dprintf("Switcheroo: HTTP client GET to set the power flags succeeded\n");
                ok = true;
            }
            
            this->http.end();           
            
            if (ok) {
                this->lastGoodSendFlags = this->powerFlags;
            }
            return ok;
        }
    public:
        SwitcherooWiFi(String hostname) {
            this->hostname = hostname;
        }
        void init() {
            this->http.setTimeout(HTTP_TIMEOUT);
            
            getPowerFlags();
        }
        bool getPowerToggle(byte portNum) {
            byte flag = 1<<portNum;
            return (flag & this->powerFlags) != 0;
        }
        void handle() {
            if (this->powerFlags != this->lastGoodSendFlags && (millis() - lastRetryStamp) > RETRY_INTERVAL_MS) {
                dbg.wprintln("Switcheroo: last sent flags don't match current flags, retrying...");
                sendFlags();
                lastRetryStamp = millis();
            }
        }
        bool setPowerToggle(byte portNum, bool on) {
            byte flag = 1<<portNum;
            if (on) {
                this->powerFlags |= flag;
            } else {
                this->powerFlags &= ~flag;
            }
            if (this->powerFlags == this->lastGoodSendFlags) {
                dbg.dprintf("Switcheroo: power flags being set match last good sent flags, skipping\n");    
                return true;
            }

            return sendFlags();            
        }
};

#endif