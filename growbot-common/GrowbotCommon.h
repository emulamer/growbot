#include <Arduino.h>
#include <Resolver.h>
#include <DebugUtils.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include "WifiManager.h"
#pragma once

#define WATCHDOG_REBOOT_SEC 45

IpResolver Resolver;

WifiManager WifiMgr;

void growbotCommonSetup(const char* hostname, const char* wifiSSID, const char* wifiPassword, std::function<void()> otaStart = NULL, std::function<void()> otaEnd = NULL, std::function<void()> otaError = NULL) {
    WifiMgr.init(hostname, wifiSSID, wifiPassword);
    dbg.println("Setting up OTA...");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA
        .onStart([otaStart]() {
            esp_task_wdt_reset();
            dbg.println("OTA update starting...");
            if (otaStart != NULL) {
                otaStart();
            }
        })
        .onEnd([otaEnd]() {
            dbg.println("Update End");
            if (otaEnd != NULL) {
                otaEnd();
            }
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            esp_task_wdt_reset();
            dbg.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([otaError](ota_error_t error) {
            esp_task_wdt_reset();
            if (otaError != NULL) {
                otaError();
            }
            dbg.printf("Error[%u]: \n", error);
            if (error == OTA_AUTH_ERROR) dbg.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) dbg.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) dbg.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) dbg.println("Receive Failed");
            else if (error == OTA_END_ERROR) dbg.println("End Failed");
        });
    ArduinoOTA.begin();
    dbg.println("Setting up mDNS...");
    MDNS.begin(hostname);
    dbg.println("Setting up watchdog ...");
    esp_task_wdt_init(WATCHDOG_REBOOT_SEC, true);
    esp_task_wdt_add(NULL); 
    dbg.println("Common setup complete!");
}

void growbotCommonLoop() {
    esp_task_wdt_reset();
    ArduinoOTA.handle();
    WifiMgr.handle();

}