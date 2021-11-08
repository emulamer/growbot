#include <Arduino.h>
#include <stdarg.h>
#include <Print.h>
#include <stdarg.h>
#include <WiFiUdp.h>
#include <WiFi.h>

#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H
#define PRINTF_BUF 200

const int udpPort = 44446;
class UdpPrint : public Print {
    private: 
    WiFiUDP udp;
    IPAddress multicastAddress = IPAddress(255, 255, 255, 255);
    bool wifiReady;


    public:
        UdpPrint() : Print() 
        {
            // _send_addr.sin_family         = AF_INET,
            // _send_addr.sin_port           = htons( 5000 );
            // _send_addr.sin_addr.s_addr    = inet_addr( "255.255.255.255" ); 
            // _send_addr.sin_len            = sizeof( _send_addr );
            // _sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );
            // int broadcast = 1;
            // setsockopt( _sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast) );
            
        }
        virtual ~UdpPrint() {}
        void wifiIsReady() {
            udp.begin(udpPort);
            wifiReady = true;
        }
        size_t write(const uint8_t *buffer, size_t size)
        {
            if (WiFi.status() == wl_status_t::WL_CONNECTED) {
                IPAddress ip = WiFi.localIP();
                IPAddress bcast = IPAddress(ip[0], ip[1], ip[2], 255);

                udp.beginPacket(bcast, udpPort);
                udp.write(buffer, size);
                udp.endPacket();
            }
            return Serial.write((const char*)buffer, size);
        }
        size_t write(uint8_t) {
            return 1;
        }


};
UdpPrint dbg;

#endif



