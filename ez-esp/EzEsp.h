#include <Arduino.h>
#include <Resolver.h>
#include <DebugUtils.h>
#include <ArduinoOTA.h>
#include "WifiManager.h"
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266mDNS.h>
#elif defined(ARDUINO_ARCH_ESP32) 
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#endif


#pragma once

#define WATCHDOG_REBOOT_SEC 35

IpResolver Resolver;

WifiManager WifiMgr;

void ezEspSetup(const char* hostname, const char* wifiSSID, const char* wifiPassword, std::function<void()> otaStart = NULL, std::function<void()> otaEnd = NULL, std::function<void()> otaError = NULL) {
    WifiMgr.init(hostname, wifiSSID, wifiPassword);
    dbg.println("Setting up OTA...");
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setRebootOnSuccess(true);
    ArduinoOTA.onStart([otaStart]() {
#ifdef ARDUINO_ARCH_ESP8266
            ESP.wdtFeed();
#elif defined(ARDUINO_ARCH_ESP32)
            esp_task_wdt_reset();
#endif
            dbg.println("OTA update starting...");
            if (otaStart != NULL) {
                otaStart();
            }
        });
    ArduinoOTA.onEnd([otaEnd]() {
            dbg.println("Update End");
            if (otaEnd != NULL) {
                otaEnd();
            }
        });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
#ifdef ARDUINO_ARCH_ESP8266
            ESP.wdtFeed();
#elif defined(ARDUINO_ARCH_ESP32)
            esp_task_wdt_reset();
#endif
            dbg.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
    ArduinoOTA.onError([otaError](ota_error_t error) {
#ifdef ARDUINO_ARCH_ESP8266
            ESP.wdtFeed();
#elif defined(ARDUINO_ARCH_ESP32)
            esp_task_wdt_reset();
#endif
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
#ifdef ARDUINO_ARCH_ESP32
    esp_task_wdt_init(WATCHDOG_REBOOT_SEC, true);
    esp_task_wdt_add(NULL); 
#endif
    dbg.println("Common setup complete!");

}
bool firstLoop = true;
void ezEspLoop() {
    if (firstLoop) {
        firstLoop = false;
        dbg.println("Safety 10 second loop to listen for upgrades in case code is garbage...");
        unsigned long start = millis();
        while (millis() - start < 10000) {
            #ifdef ARDUINO_ARCH_ESP8266
                ESP.wdtFeed();
            #elif defined(ARDUINO_ARCH_ESP32)
                esp_task_wdt_reset();
            #endif
            ArduinoOTA.handle();
            WifiMgr.handle();    
        }
        dbg.println("Safety start done, continuing startup");
    }
#ifdef ARDUINO_ARCH_ESP8266
    ESP.wdtFeed();
#elif defined(ARDUINO_ARCH_ESP32)
    esp_task_wdt_reset();
#endif
    ArduinoOTA.handle();
    WifiMgr.handle();
#ifdef ARDUINO_ARCH_ESP8266
    if (WiFi.isConnected()) {
        MDNS.update();
    }
#endif
}