#include <Arduino.h>
#include <stdarg.h>
#include <Print.h>
#include <stdarg.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUDP.h>
#elif defined(ARDUINO_ARCH_ESP32) 
#include <Wifi.h>
#include <WiFiUdp.h>
#endif


#pragma once

#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

#define PRINTF_BUF 200 
class SerialPrint : public Print {
    private:
      byte logLevel = LOG_LEVEL_DEBUG;
    public:
        SerialPrint() : Print() 
        { }
        virtual ~SerialPrint() {}
        void wifiIsReady() {}
        void setLogLevel(byte level) {
          logLevel = level;
        }
        size_t write(const uint8_t *buffer, size_t size)
        {
            return Serial.write((const char*)buffer, size);
        }
        size_t write(uint8_t i)
        {
            return Serial.write(i);
        }
        template<typename... Args> void printf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename... Args> void dprintf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename... Args> void eprintf(const char * format, Args... args) {
          Print::printf(format, args...);
        }
        template<typename... Args> void wprintf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename Arg> void println(Arg msg) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void eprintln(Arg msg) {
          Print::println(msg);
        }
        template<typename Arg> void wprintln(Arg msg) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void dprintln(Arg msg) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void print(Arg msg) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::print(msg);
        }
        template<typename Arg> void eprint(Arg msg) {
          Print::print(msg);
        }
        template<typename Arg> void wprint(Arg msg) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::print(msg);
        }
        template<typename Arg> void dprint(Arg msg) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::print(msg);
        }
};

#ifdef LOG_UDP_PORT

class UdpPrint : public Print {
    private: 
      byte logLevel = LOG_LEVEL_DEBUG;
      WiFiUDP udp;
      IPAddress multicastAddress = IPAddress(255, 255, 255, 255);
      bool wifiReady;
    public:
        UdpPrint() : Print() 
        { }

        virtual ~UdpPrint() {}
        void wifiIsReady() {
            udp.begin(LOG_UDP_PORT);
            wifiReady = true;
        }
        void setLogLevel(byte level) {
          logLevel = level;
        }
        size_t write(const uint8_t *buffer, size_t size)
        {
            if (wifiReady) {
                IPAddress ip = WiFi.localIP();
                IPAddress bcast = IPAddress(ip[0], ip[1], ip[2], 255);

                udp.beginPacket(bcast, LOG_UDP_PORT);
                udp.write(buffer, size);
                udp.endPacket();
                delay(10);
            }
#ifndef NO_SERIAL
            return Serial.write((const char*)buffer, size);
#else
            return size;
#endif
        }
        size_t write(uint8_t) {
            return 1;
        }
        template<typename... Args> void printf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename... Args> void dprintf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename... Args> void eprintf(const char * format, Args... args) {
          Print::printf(format, args...);
        }
        template<typename... Args> void wprintf(const char * format, Args... args) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::printf(format, args...);
        }
        template<typename Arg> void println(Arg msg) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void eprintln(Arg msg) {
          Print::println(msg);
        }
        template<typename Arg> void wprintln(Arg msg) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void dprintln(Arg msg) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::println(msg);
        }
        template<typename Arg> void print(Arg msg) {
          if (logLevel < LOG_LEVEL_INFO) {
            return;
          }
          Print::print(msg);
        }
        template<typename Arg> void eprint(Arg msg) {
          Print::print(msg);
        }
        template<typename Arg> void wprint(Arg msg) {
          if (logLevel < LOG_LEVEL_WARN) {
            return;
          }
          Print::print(msg);
        }
        template<typename Arg> void dprint(Arg msg) {
          if (logLevel < LOG_LEVEL_DEBUG) {
            return;
          }
          Print::print(msg);
        }

};

UdpPrint dbg;
#else
SerialPrint dbg;
#endif




