#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Print.h>
#include <WiFiUdp.h>
#include <Wifi.h>
#ifndef DEBUGUTILS_H
#define DEBUGUTILS_H
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

};
UdpPrint dbg;


void printHex(uint8_t num) {
  char hexCar[2];

  sprintf(hexCar, "%02X", num);
  dbg.print(hexCar);
}
void debug_find_onewire_sensors(OneWire oneWire) {
  oneWire.reset_search();
  DeviceAddress addr;
  while (oneWire.search(addr)) {
    dbg.print("Onewire device found, address: ");
    for (byte i = 0; i < 8; i++) {
      dbg.print("0x");
      printHex(addr[i]);
      dbg.print(", ");
    }
    dbg.println("");
  }
} 

void debug_scan_i2c() {
  byte error, address; //variable for error and I2C address
  int nDevices;

  dbg.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      dbg.print("I2C device found at address 0x");
      if (address < 16)
        dbg.print("0");
      dbg.print(address, HEX);
      dbg.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      dbg.print("Unknown error at address 0x");
      if (address < 16)
        dbg.print("0");
      dbg.println(address, HEX);
    }
  }
  if (nDevices == 0)
    dbg.println("No I2C devices found\n");
  else
    dbg.println("done\n");
}
#endif