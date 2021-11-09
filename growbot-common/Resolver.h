#include <Arduino.h>
#include "DebugUtils.h"
#include <ESPmDNS.h>
#include <ESP32Ping.h>
#include <regex>
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
    const std::regex ipRegex = std::regex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");
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
            if (std::regex_match(hostname.c_str(), ipRegex)) {
             //   dbg.dprintf("%s appears to be a raw ip address in there\n", hostname.c_str());
                unsigned int parts[4];
                int got = sscanf(hostname.c_str(), "%u.%u.%u.%u", &parts[0], &parts[1], &parts[2], &parts[3]);
                IPAddress ip;
                if (got != 4 || parts[0] > 255 || parts[1] > 255 || parts[2] > 255 || parts[3] > 255) {
             //       dbg.dprintf("Only got %d expected 4, or one of them is over 255\n", got);
                } else {
                    ip[0] = parts[0];
                    ip[1] = parts[1];
                    ip[2] = parts[2];
                    ip[3] = parts[3];
               //     dbg.dprintf("%s seemed to parse out ok to %s\n", hostname.c_str(), ip.toString());
                    ref->ip = ip;
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
               // dbg.dprintf("Resolver: DNS failed, trying to resolving hostname %s via mDNS...\n", ref->hostname);
                resolvedIp = MDNS.queryHost(ref->hostname);
                if (resolvedIp.toString() == "0.0.0.0") {
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

