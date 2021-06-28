#include <Arduino.h>
#include <stdarg.h>
#include <Print.h>
#include <Wire.h>
//#include <OneWire.h>
#include <DallasTemperature.h>
#include <stdarg.h>



#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H
#define PRINTF_BUF 200 // define the tmp buffer size (change if desired)
class SerialPrint : public Print {
    public:
        SerialPrint() : Print() 
        { }
        virtual ~SerialPrint() {}
        
        size_t write(const uint8_t *buffer, size_t size)
        {
            //sendto(_sock,buffer, size, 0, (const struct sockaddr*) &_send_addr, sizeof(_send_addr));
            return Serial.write((const char*)buffer, size);
        }
        size_t write(uint8_t a) {
          return write(&a, 1);
        }

        void printf(const char *format, ...)
        {
          char buf[PRINTF_BUF];
          va_list ap;
          va_start(ap, format);
          int len = vsnprintf(buf, sizeof(buf), format, ap);
          write((const uint8_t*)buf, len );
          va_end(ap);
        }
};


#ifdef ARDUINO_ARCH_ESP32
#include <WiFiUdp.h>
#include <Wifi.h>
const char * udpAddress = "192.168.1.219";
const int udpPort = 44444;
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
            if (wifiReady) {
                IPAddress ip = WiFi.localIP();
                IPAddress bcast = IPAddress(ip[0], ip[1], ip[2], 255);

                udp.beginPacket(bcast, udpPort);
                udp.write(buffer, size);
                udp.endPacket();
            }
            //sendto(_sock,buffer, size, 0, (const struct sockaddr*) &_send_addr, sizeof(_send_addr));
            return Serial.write((const char*)buffer, size);
        }
        size_t write(uint8_t) {
            return 1;
        }
        // size_t println(const char* msg) {
        //   int len = strlen(msg);
        //   char *bfr = (char *)malloc(len +1);
        //   strcpy((char *)msg, bfr);
        //   bfr[len] = '\n';
        //   this->write((const uint8_t*)bfr, len + 1);
        //   free((void *)bfr);
        //   return len + 1;
        // }

};
UdpPrint dbg;
#else
SerialPrint dbg;
#endif

void printHex(uint8_t num) {
  char hexCar[2];

  sprintf(hexCar, "%02X", num);
  dbg.print(hexCar);
}
// void debug_find_onewire_sensors(OneWire oneWire) {
//   oneWire.reset_search();
//   DeviceAddress addr;
//   while (oneWire.search(addr)) {
//     dbg.print("Onewire device found, address: ");
//     for (byte i = 0; i < 8; i++) {
//       dbg.print("0x");
//       printHex(addr[i]);
//       dbg.print(", ");
//     }
//     dbg.println("");
//   }
// } 


// void debug_find_i2c(byte port, I2CMultiplexer &plexer) {
//   for (int i = 0; i < 8; i++) {
//     dbg.printf("Selecting bus %d...\n", i);
//     plexer.setBus(i);
//     debug_scan_i2c(Wire, port, i);
//   }
// }
static bool millisElapsed(unsigned long ms) {
            if (ms <= millis()) {
            return true;
            }
            //todo: need to make sure this handles the 52 day wrap here or things will break
            if (abs(ms - millis()) > 10000000) {
            return true;
            }
            return false;
        }
#endif



