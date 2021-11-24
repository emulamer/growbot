#include <Arduino.h>
#include "DebugUtils.h"
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266mDNS.h>
#include <ESP8266Ping.h>
#include <mDNSResolver.h>
#include <WiFiUDP.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#endif
#include "lwip/dns.h"

#pragma once
//introduce a random amount so not all of the pings happen at the same time
#define RANDOM_AMOUNT_MS 20000
#define PING_CHECK_INTERVAL_MS 120000
#define RESOLVE_EXPIRE_MS 3600000

struct ResolveRef {
    const char* hostname = NULL;
    IPAddress ip = INADDR_ANY;
    unsigned long resolvedStamp = 0;
    unsigned long lastPingStamp = 0;
    bool isResolved = false;
    bool isMDNS = false;
};

#ifdef ARDUINO_ARCH_ESP8266
    WiFiUDP _udp_8266;
    mDNSResolver::Resolver _resolver_8266(_udp_8266);
#endif

class IpResolver {
private:

    std::vector<ResolveRef*> resolved;
    bool ping(IPAddress ip) {
        for (int i = 0; i < 3; i++) {
            if (Ping.ping(ip, 1)) {
                return true;
            }
        }
        return false;
    }
public:
    bool resolve(String hostname, IPAddress* resolvedToIp) {
        ResolveRef* ref = NULL;
        bool found = false;
        const char* hostcstr = hostname.c_str();
        for (auto res : this->resolved) {
            if (strcmp(res->hostname, hostcstr) == 0) {
                ref = res;
                found = true;
                break;
            }
        }
        
        if (!found) {
           // dbg.dprintf("Resolver: %s is not yet a known host\n", hostcstr);
            ref = new ResolveRef();
            int len = strlen(hostcstr)+1;
            ref->hostname = (const char*)malloc(len);
            memcpy((void*)ref->hostname, hostcstr, len);
            this->resolved.push_back(ref);
            const char* ipStr = hostname.c_str();
            IPAddress ipSegs;
            len--;
            int segIdx = 0;
            bool parseFail = false;
            int numStartIdx = -1;
            for (int i = 0; i < len; i++) {
                if (ipStr[i] >= '0' && ipStr[i] <= '9') {
                    if (numStartIdx < 0) {
                        numStartIdx = i;
                    }
                } else if (ipStr[i] == '.') {
                    if (numStartIdx < 1) {
                        parseFail = true;
                        break;
                    } else {
                        int ctr = 0;
                        ipSegs[segIdx] = 0;
                        for (int j = i; j >= numStartIdx; j--) {
                            ipSegs[segIdx] += pow(10, ctr) * (ipStr[j] - '0');
                        }
                        numStartIdx = -1;
                        segIdx++;
                        if (segIdx > 3) {
                            parseFail = true;
                            break;
                        }
                    }
                }
            }
            if (!parseFail && numStartIdx >= 0 && segIdx == 2) {
                int ctr = 0;
                ipSegs[segIdx] = 0;
                for (int j = len-1; j >= numStartIdx; j--) {
                    ipSegs[segIdx] += pow(10, ctr) * (ipStr[j] - '0');
                }
            } else {
                parseFail = true;
            }

            if (!parseFail && segIdx != 3) {
                parseFail = true;
            }
            if (!parseFail) {
                   if (ipSegs[0] > 255 || ipSegs[1] > 255 || ipSegs[2] > 255 || ipSegs[3] > 255) {
                //      dbg.dprintf("one of them is over 255\n");
                } else {
                  //   dbg.dprintf("%s seemed to parse out ok to %s\n", hostname.c_str(), ipSegs.toString());
                    ref->ip = ipSegs;
                    ref->isResolved = true;
                    ref->resolvedStamp = millis();
                }            
            }
        }
        
        if (ref->isResolved && ref->isMDNS) {
            if (millis() - ref->resolvedStamp > RESOLVE_EXPIRE_MS) {
           //     dbg.dprintf("Resolver: resolution has expired for host %s\n", ref->hostname);
                ref->isResolved = false;
            } else if (millis() - ref->lastPingStamp > PING_CHECK_INTERVAL_MS) {
            //    dbg.dprintf("Resolver: checking that resolved host %s (%s) pings...\n", ref->hostname, ref->ip.toString().c_str());
                if (!ping(ref->ip)) {
                    dbg.wprintf("Resolver: host %s (%s) did NOT ping!\n", ref->hostname, ref->ip.toString().c_str());
                    ref->isResolved = false;
                } else {
           //         dbg.dprintf("Resolver: host %s (%s) ping check passed!\n", ref->hostname, ref->ip.toString().c_str());
                    ref->lastPingStamp = millis() + random(RANDOM_AMOUNT_MS);
                }
            }
        }
        if (!ref->isResolved) {
          //  dbg.dprintf("Resolver: resolving hostname %s...\n", ref->hostname);
            IPAddress resolvedIp;
         //   dbg.dprintf("Resolver: trying to resolve hostname %s via DNS ...\n", ref->hostname);
            if (!ref->isMDNS && WiFi.hostByName(ref->hostname, resolvedIp) == 1) {
               // dbg.dprintf("Resolver: resolved hostname %s to ip %s via DNS ...\n", ref->hostname, resolvedIp.toString().c_str());
                ref->isMDNS = false;
            } else {
#ifdef ARDUINO_ARCH_ESP8266
                String resoName = String(ref->hostname);
                resoName = resoName + ".local";

                resolvedIp = _resolver_8266.search(resoName.c_str());
                if(resolvedIp == INADDR_NONE) {
#elif defined(ARDUINO_ARCH_ESP32)
               // dbg.dprintf("Resolver: DNS failed, trying to resolving hostname %s via mDNS...\n", ref->hostname);
                resolvedIp = MDNS.queryHost(ref->hostname);
                if (resolvedIp.toString() == "0.0.0.0") {
#endif
                    dbg.wprintf("Resolver: MDNS failed to resolve hostname %s!\n", ref->hostname);
                    return false;
                }
                //dbg.dprintf("Resolver: MDNS resolved %s to IP %s, checking pingability...\n", ref->hostname, resolvedIp.toString().c_str());
                if (!ping(resolvedIp)) {
                    dbg.wprintf("Resolver: host %s (%s) did NOT ping after resolution!\n", ref->hostname, resolvedIp.toString().c_str());
                    return false;
                }
                ref->isMDNS = true;
            }

            ref->ip = resolvedIp;
            ref->resolvedStamp = millis() + random(RANDOM_AMOUNT_MS);
            ref->lastPingStamp = millis() + random(RANDOM_AMOUNT_MS);
            ref->isResolved = true;
            
            dbg.printf("Resolver: host %s resolved to IP %s\n", ref->hostname, resolvedIp.toString().c_str());
        }
        *resolvedToIp = ref->ip;
        return true;        
    }
};

