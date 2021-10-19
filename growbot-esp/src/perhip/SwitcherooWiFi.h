#include <Arduino.h>
#include <Wire.h>
#include "I2CMultiplexer.h"
#include <HTTPClient.h>
#include <ESPmDNS.h>

#ifndef SWITCHEROO_H
#define SWITCHEROO_H

#define HTTP_TIMEOUT 3000

class SwitcherooWiFi {
    private: 
        String hostname;
        String* baseUrl = NULL;
        byte powerFlags = 0;
        HTTPClient http;
        byte lastGoodSendFlags = -1;
        bool checkBaseUrl() {
            if (this->baseUrl != NULL) {
                return true;
            }
            dbg.printf("Switcheroo: hostname hasn't been resolved yet...\n");
            IPAddress serverIp = MDNS.queryHost(this->hostname);
            String ipStr = serverIp.toString();
            if (ipStr == "0.0.0.0") {
                dbg.printf("Switcheroo: MDNS failed to resolve hostname!\n");
                return false;
            }
            dbg.printf("Switcheroo: MDNS resolved hostname to %s\n", ipStr.c_str());
            this->baseUrl = new String(String("http://") + ipStr + String("/"));
            dbg.printf("Switcheroo: base url set to %s\n", this->baseUrl->c_str());
            return true;
        }
        bool getPowerFlags() {
            if (!this->checkBaseUrl()) {
                return false;
            }
            dbg.printf("Switcheroo: HTTP client making request to url %s\n", this->baseUrl->c_str());
            if (!this->http.begin(this->baseUrl->c_str())) {
                dbg.printf("Switcheroo: HTTP client begin returned false!\n");
                return false;
            }
            bool ok = false;
            int code = this->http.GET();
            if (code != 200) {
                dbg.printf("Switcheroo: HTTP client GET failed with status %d!\n", code);
            } else {
                String resp = http.getString();
                if (resp.length() < 1 || resp.length() > 3) {
                    dbg.printf("Switcheroo: HTTP client GET response was wrong length, resp: %s\n", resp.c_str());    
                } else {
                    bool bad = false;
                    for (int i = 0; i < resp.length(); i++) {
                        if (!isDigit(resp[i])) {
                            bad = true;
                            break;
                        }
                    }
                    if (bad) {
                        dbg.printf("Switcheroo: HTTP client GET response was wrong not numeric, resp: %s\n", resp.c_str());
                        ok = false;
                    } else {
                        int portStatusNum = resp.toInt();
                        dbg.printf("Switcheroo: HTTP client got port status of %d\n", portStatusNum);
                        if (portStatusNum < 0 || portStatusNum > 255) {
                            dbg.printf("Switcheroo: HTTP client got out of range number %d\n", portStatusNum);
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
        bool setPowerToggle(byte portNum, bool on) {
            byte flag = 1<<portNum;
            if (on) {
                this->powerFlags |= flag;
            } else {
                this->powerFlags &= ~flag;
            }
            if (this->powerFlags == this->lastGoodSendFlags) {
                dbg.printf("Switcheroo: power flags being set match last good sent flags, skipping\n");    
                return true;
            }
            if (!this->checkBaseUrl()) {
                return false;
            }
            dbg.printf("Switcheroo: setting power flags: %d\n", this->powerFlags);
            String url = *this->baseUrl + String(this->powerFlags);
            dbg.printf("Switcheroo: HTTP client making request to url %s\n", url.c_str());
             if (!this->http.begin(url.c_str())) {
                dbg.printf("Switcheroo: HTTP client begin returned false!\n");
                return false;
            }
            bool ok = false;
            int code = this->http.GET();
            if (code != 200) {
                dbg.printf("Switcheroo: HTTP client GET failed with status %d!\n", code);
            } else {
                dbg.printf("Switcheroo: HTTP client GET to set the power flags succeeded\n");
                ok = true;
            }
            this->http.end();
            if (ok) {
                this->lastGoodSendFlags = this->powerFlags;
            }
            return ok;
        }
};

#endif